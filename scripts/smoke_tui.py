#!/usr/bin/env python3
"""Reproducible TUI smoke test for apl1 (FH-01).

Runs the real application in a real terminal emulator (xterm on a private
Xvfb display), checks the startup screen, resizes the window and checks that
Notcurses redraws, sends the quit key, and verifies a clean exit and an
isolated database. It is deliberately not a terminal-compatibility suite: the
only simulated part is the window manager key/resize request from xdotool.

Why xterm rather than tmux? Notcurses 3.0 interrogates the terminal for its
palette. A detached tmux server (or screen) does not answer those queries, so
Notcurses blocks during initialization. xterm is a full terminal emulator and
answers them, which makes the harness deterministic and headless.

Dependencies: Python 3, Xvfb, xterm, and xdotool.

Examples:
    scripts/smoke_tui.py                         # find ./build/apl1 or cmake-build-*
    scripts/smoke_tui.py --app build/apl1 --timeout 30
    scripts/smoke_tui.py --output /tmp/apl1-smoke.log
"""

import argparse
import os
import re
import shlex
import shutil
import signal
import sqlite3
import subprocess
import sys
import tempfile
import time
import uuid
from pathlib import Path

DEFAULT_TIMEOUT = 20.0
ANSI_ESCAPE = re.compile(r"\x1b(?:\[[0-9;?]*[ -/]*[@-~]|\([A-Za-z0-9]|[=>])")
APP_CANDIDATES = (
    "build/apl1",
    "cmake-build-debug/apl1",
    "cmake-build-release/apl1",
    "cmake-build-relwithdebinfo/apl1",
)


class SmokeFailure(Exception):
    """A failed assertion with a human-readable explanation."""


class Smoke:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.events: list[str] = []
        self.temp_dir = Path(tempfile.mkdtemp(prefix="apl1-smoke-"))
        self.database_path = self.temp_dir / "smoke.sqlite3"
        self.status_path = self.temp_dir / "app.status"
        self.xterm_log = self.temp_dir / "xterm.log"
        self.xvfb_log = self.temp_dir / "xvfb.log"
        self.app = args.app
        self.timeout = args.timeout
        self.title = f"apl1-smoke-{os.getpid()}-{uuid.uuid4().hex[:8]}"
        self.launch_count = 0
        self.display = ""
        self.display_env: dict[str, str] = {}
        self.xvfb: subprocess.Popen[bytes] | None = None
        self.xterm: subprocess.Popen[bytes] | None = None
        self.window_id = ""
        self.cell_width = 0.0
        self.cell_height = 0.0

    # ---------------------------------------------------------------- logging
    def log(self, message: str) -> None:
        line = f"[smoke] {message}"
        self.events.append(line)
        print(line, flush=True)

    def fail(self, message: str) -> None:
        raise SmokeFailure(message)

    def read_xterm_log(self) -> str:
        try:
            return self.xterm_log.read_bytes().decode("utf-8", "replace")
        except FileNotFoundError:
            return ""

    def visible_text(self) -> str:
        """Return terminal output with ANSI control sequences replaced by spaces.

        Notcurses skips over unchanged space cells instead of writing them, so
        raw logs split phrases at cursor movements. Normalizing to collapsed
        whitespace lets assertions check the visible text, not the byte stream.
        """
        text = ANSI_ESCAPE.sub(" ", self.read_xterm_log())
        return " ".join(text.split())

    # ------------------------------------------------------------- subprocess
    def run(self, command: list[str], *, env: dict[str, str] | None = None) -> str:
        result = subprocess.run(
            command,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=self.timeout,
        )
        if result.returncode != 0:
            detail = (result.stderr or result.stdout).strip()
            self.fail(f"command failed ({result.returncode}): {shlex.join(command)}: {detail}")
        return result.stdout

    def wait_for(self, description: str, predicate, *, timeout: float | None = None) -> None:
        limit = self.timeout if timeout is None else timeout
        deadline = time.monotonic() + limit
        while time.monotonic() < deadline:
            if predicate():
                return
            time.sleep(0.05)
        self.fail(f"timed out after {limit:g}s waiting for {description}")

    # --------------------------------------------------------------- display
    def start_display(self) -> None:
        for number in range(90, 130):
            socket_path = Path(f"/tmp/.X11-unix/X{number}")
            lock_path = Path(f"/tmp/.X{number}-lock")
            if socket_path.exists() or lock_path.exists():
                continue
            display = f":{number}"
            with self.xvfb_log.open("wb") as log:
                process = subprocess.Popen(
                    [
                        "Xvfb",
                        display,
                        "-screen",
                        "0",
                        "1024x768x24",
                        "-nolisten",
                        "tcp",
                        "-ac",
                    ],
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    start_new_session=True,
                )
            try:
                self.wait_for(
                    f"Xvfb display {display}",
                    lambda: socket_path.exists() and process.poll() is None,
                    timeout=10.0,
                )
            except SmokeFailure:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)
                continue
            self.display = display
            self.xvfb = process
            self.display_env = {"DISPLAY": display}
            self.log(f"started Xvfb on {display}")
            return
        self.fail("could not allocate a free Xvfb display (tried :90 through :129)")

    # ---------------------------------------------------------------- xterm
    def start_xterm(self, geometry: str = "80x24") -> None:
        self.launch_count += 1
        self.title = (
            f"apl1-smoke-{os.getpid()}-{uuid.uuid4().hex[:8]}-{self.launch_count}"
        )
        self.xterm_log = self.temp_dir / f"xterm-{self.launch_count}.log"
        self.status_path.unlink(missing_ok=True)
        env = os.environ.copy()
        env.update(
            {
                "DISPLAY": self.display,
                "XDG_DATA_HOME": str(self.temp_dir / "xdg"),
                "HOME": str(self.temp_dir / "home"),
                "TERM": "xterm-256color",
                "LANG": "C.UTF-8",
                "LC_ALL": "C.UTF-8",
            }
        )
        (self.temp_dir / "xdg").mkdir(exist_ok=True)
        (self.temp_dir / "home").mkdir(exist_ok=True)

        app_command = (
            f"{shlex.quote(str(self.app))} --database {shlex.quote(str(self.database_path))}; "
            f"status=$?; printf '%s' \"$status\" > {shlex.quote(str(self.status_path))}; "
            f"exit \"$status\""
        )
        stdio_log = self.temp_dir / f"xterm-{self.launch_count}-stdio.log"
        log = stdio_log.open("wb")
        self.xterm = subprocess.Popen(
            [
                "xterm",
                "-name",
                self.title,
                "-title",
                self.title,
                "-geometry",
                geometry,
                "-l",
                "-lf",
                str(self.xterm_log),
                "-e",
                "sh",
                "-c",
                app_command,
            ],
            env=env,
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
        log.close()
        self.wait_for("xterm window", self._find_window, timeout=10.0)
        geometry = self.run(
            ["xdotool", "getwindowgeometry", "--shell", self.window_id],
            env=self.display_env,
        )
        width = height = 0
        for line in geometry.splitlines():
            if line.startswith("WIDTH="):
                width = int(line.split("=", 1)[1])
            elif line.startswith("HEIGHT="):
                height = int(line.split("=", 1)[1])
        if width <= 0 or height <= 0:
            self.fail(f"could not read the initial xterm geometry: {geometry!r}")
        self.cell_width = width / 80
        self.cell_height = height / 24
        time.sleep(0.4)
        self.run(
            ["xdotool", "windowfocus", "--sync", self.window_id],
            env=self.display_env,
        )
        self.log(f"started app in xterm window {self.window_id}")

    def _find_window(self) -> bool:
        result = subprocess.run(
            ["xdotool", "search", "--name", f"^{self.title}$"],
            env=self.display_env,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        identifiers = result.stdout.split()
        if not identifiers:
            return False
        self.window_id = identifiers[0]
        return True

    def wait_for_content(self, description: str, needles: tuple[str, ...]) -> None:
        def present() -> bool:
            text = self.visible_text()
            return all(needle in text for needle in needles)

        self.wait_for(description, present, timeout=10.0)

    def send_keys(self, *keys: str) -> None:
        self.run(
            ["xdotool", "windowfocus", "--sync", self.window_id],
            env=self.display_env,
        )
        for key in keys:
            self.run(["xdotool", "key", key], env=self.display_env)
            time.sleep(0.08)

    def click_cell(self, row: int, column: int) -> None:
        self.run(
            ["xdotool", "windowfocus", "--sync", self.window_id],
            env=self.display_env,
        )
        geometry = self.run(
            ["xdotool", "getwindowgeometry", "--shell", self.window_id],
            env=self.display_env,
        )
        x_origin = y_origin = width = height = 0
        for line in geometry.splitlines():
            if line.startswith("X="):
                x_origin = int(line.split("=", 1)[1])
            elif line.startswith("Y="):
                y_origin = int(line.split("=", 1)[1])
            elif line.startswith("WIDTH="):
                width = int(line.split("=", 1)[1])
            elif line.startswith("HEIGHT="):
                height = int(line.split("=", 1)[1])
        if width <= 0 or height <= 0 or self.cell_width <= 0 or self.cell_height <= 0:
            self.fail(f"could not read xterm geometry for a click: {geometry!r}")
        x = x_origin + int(self.cell_width * (column + 0.5))
        y = y_origin + int(self.cell_height * (row + 0.5))
        self.run(
            ["xdotool", "mousemove", "--sync", str(x), str(y), "click", "1"],
            env=self.display_env,
        )
        time.sleep(0.15)

    def type_text(self, text: str) -> None:
        self.run(
            ["xdotool", "windowfocus", "--sync", self.window_id],
            env=self.display_env,
        )
        self.run(["xdotool", "type", "--delay", "20", text], env=self.display_env)
        time.sleep(0.15)

    def database_query(self, sql: str):
        with sqlite3.connect(self.database_path) as connection:
            return connection.execute(sql).fetchone()

    def wait_for_database(self, description: str, predicate) -> None:
        def ready() -> bool:
            if not self.database_path.is_file():
                return False
            try:
                return bool(predicate())
            except sqlite3.Error:
                return False

        self.wait_for(description, ready, timeout=10.0)

    def resize_and_check_redraw(self) -> None:
        geometry = self.run(
            ["xdotool", "getwindowgeometry", "--shell", self.window_id],
            env=self.display_env,
        )
        width = height = 0
        for line in geometry.splitlines():
            if line.startswith("WIDTH="):
                width = int(line.split("=", 1)[1])
            elif line.startswith("HEIGHT="):
                height = int(line.split("=", 1)[1])
        if width <= 0 or height <= 0:
            self.fail(f"could not read xterm geometry: {geometry!r}")

        log_size_before = len(self.read_xterm_log())

        new_width = width + 200
        new_height = height + 100
        self.run(
            ["xdotool", "windowsize", self.window_id, str(new_width), str(new_height)],
            env=self.display_env,
        )
        self.log(f"resized xterm window from {width}x{height} to {new_width}x{new_height}")

        # A key wakes the blocking input loop so the next frame is emitted at
        # the new geometry even on a bare Xvfb server without a window manager.
        self.send_keys("Escape")
        self.wait_for(
            "output after resize",
            lambda: len(self.read_xterm_log()) > log_size_before,
            timeout=10.0,
        )
        self.wait_for_content(
            "startup content after resize",
            ("No teams have been added yet.",),
        )

    def quit(self) -> None:
        subprocess.run(
            ["xdotool", "windowfocus", "--sync", self.window_id],
            env=self.display_env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        self.send_keys("q")
        self.log("sent q to the application")

        def exited() -> bool:
            finished = self.xterm is not None and self.xterm.poll() is not None
            status_ready = (
                self.status_path.exists() and self.status_path.read_text().strip() != ""
            )
            return finished and status_ready

        self.wait_for("application exit", exited, timeout=10.0)

    def check_exit_status(self) -> None:
        if not self.status_path.exists():
            self.fail("application did not record an exit status (crashed or was killed)")
        status = self.status_path.read_text().strip()
        if status != "0":
            self.fail(f"application exited with status {status}, expected 0")

    def check_window_cleanup(self) -> None:
        def window_gone() -> bool:
            result = subprocess.run(
                ["xdotool", "search", "--name", f"^{self.title}$"],
                env=self.display_env,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True,
            )
            return not result.stdout.strip()

        self.wait_for("xterm window cleanup", window_gone, timeout=10.0)

    def check_database(self) -> None:
        if not self.database_path.is_file():
            self.fail(f"expected database was not created: {self.database_path}")
        try:
            with sqlite3.connect(self.database_path) as connection:
                version = connection.execute(
                    "SELECT COALESCE(MAX(version), 0) FROM apl_schema_migrations"
                ).fetchone()[0]
        except sqlite3.Error as error:
            self.fail(f"database is not a usable migrated apl1 database: {error}")
        if version != 3:
            self.fail(f"database schema version is {version}, expected 3")

        with sqlite3.connect(self.database_path) as connection:
            teams = connection.execute(
                "SELECT name, archived FROM teams ORDER BY id"
            ).fetchall()
            active = connection.execute(
                "SELECT t.name FROM app_settings s "
                "JOIN teams t ON t.id = s.active_team_id WHERE s.id = 1"
            ).fetchone()
        if teams != [("Ferrari", 0), ("McLaren", 0)]:
            self.fail(f"unexpected team rows after smoke run: {teams!r}")
        if active != ("Ferrari",):
            self.fail(f"active team after smoke run is {active!r}, expected Ferrari")

        default_database = self.temp_dir / "xdg" / "apl1" / "apl1.sqlite3"
        if default_database.exists():
            self.fail(
                f"the environment-derived database was created at {default_database} "
                "even though --database was supplied"
            )

    # ---------------------------------------------------------------- cleanup
    def stop_processes(self) -> None:
        for name, process in (("xterm", self.xterm), ("Xvfb", self.xvfb)):
            if process is None or process.poll() is not None:
                continue
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                continue
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.wait(timeout=5)
            self.log(f"terminated {name}")

    def write_output(self, *, failed: bool) -> Path | None:
        if self.args.output is None and not failed and not self.args.keep:
            return None
        if self.args.output is not None:
            destination = Path(self.args.output).expanduser()
            destination.parent.mkdir(parents=True, exist_ok=True)
            mode = "w"
        else:
            destination = self.temp_dir / "smoke.log"
            mode = "w"
        with destination.open(mode, encoding="utf-8") as output:
            output.write("\n".join(self.events))
            output.write("\n\n----- xterm output (raw) -----\n")
            output.write(self.read_xterm_log())
        return destination

    def run_checks(self) -> None:
        self.start_display()
        self.start_xterm()
        self.wait_for_content(
            "startup screen",
            (
                "Welcome to apl1.",
                "No teams have been added yet.",
                "Press n to create your first team.",
            ),
        )
        self.log("startup content is present")
        self.resize_and_check_redraw()
        self.log("resize produced a redraw")
        self.wait_for_content(
            "window bar labels",
            ("Overview", "Settings", "Help", "About"),
        )
        self.wait_for_content(
            "vertical tab rail",
            ("Teams", "People", "Money", "History"),
        )
        self.click_cell(3, 3)
        self.wait_for_content("mouse tab switch", ("People is not implemented in this phase.",))
        self.click_cell(2, 3)
        self.wait_for_content("tab return to Teams", ("No teams have been added yet.",))
        self.send_keys("Escape")
        self.log("switched tabs by mouse and returned to Teams")
        self.send_keys("m")
        self.wait_for_content("keyboard menu dropdown", ("Active team overview",))
        self.send_keys("Escape", "Escape")
        self.log("opened and closed the window menu by keyboard")
        self.click_cell(0, 3)
        self.wait_for_content("mouse menu dropdown", ("Active team overview",))
        self.send_keys("Escape", "Escape")
        self.log("opened and closed the window menu by mouse")

        self.send_keys("n")
        self.type_text("Ferrari")
        self.send_keys("Tab")
        self.type_text("Maranello")
        self.send_keys("Return")
        self.wait_for_database(
            "first team row",
            lambda: self.database_query("SELECT COUNT(*) FROM teams;") == (1,),
        )
        self.wait_for_content("creation detail", ("Name: Ferrari",))
        self.log("created Ferrari through the form")

        self.send_keys("Escape", "n")
        self.type_text("McLaren")
        self.send_keys("Return")
        self.wait_for_database(
            "second team row",
            lambda: self.database_query("SELECT COUNT(*) FROM teams;") == (2,),
        )
        self.log("created McLaren through the form")

        self.send_keys("Escape", "Up", "s")
        self.wait_for_database(
            "switched active team",
            lambda: self.database_query(
                "SELECT t.name FROM app_settings s "
                "JOIN teams t ON t.id = s.active_team_id WHERE s.id = 1;"
            )
            == ("Ferrari",),
        )
        self.log("switched active team to Ferrari")

        self.quit()
        self.check_exit_status()
        self.log("application exited with status 0")
        self.check_window_cleanup()
        self.log("xterm window was cleaned up")

        # Relaunch the same database and verify the selected team persists.
        self.start_xterm()
        self.wait_for_content("relaunch screen", ("Ferrari",))
        if "No teams have been added yet." in self.read_xterm_log():
            self.fail("relaunch incorrectly showed the first-run welcome screen")
        self.wait_for_database(
            "persisted active team after relaunch",
            lambda: self.database_query(
                "SELECT t.name FROM app_settings s "
                "JOIN teams t ON t.id = s.active_team_id WHERE s.id = 1;"
            )
            == ("Ferrari",),
        )
        self.log("relaunch shows the persisted active team")
        self.quit()
        self.check_exit_status()
        self.check_window_cleanup()
        self.check_database()
        self.log("isolated database was created, migrated, and retained team state")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run the apl1 TUI startup/resize/quit smoke test in xterm on Xvfb.",
    )
    parser.add_argument(
        "--app",
        type=Path,
        help="path to the apl1 executable (default: search common build directories)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT,
        help=f"per-step timeout in seconds (default: {DEFAULT_TIMEOUT:g})",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="write the harness log and raw terminal output to this file "
        "(default: temporary directory, kept on failure)",
    )
    parser.add_argument(
        "--keep",
        action="store_true",
        help="keep the temporary directory (database and logs) after success",
    )
    return parser.parse_args(argv)


def find_app(requested: Path | None) -> Path:
    if requested is not None:
        candidates = [requested]
    else:
        candidates = [Path(candidate) for candidate in APP_CANDIDATES]
    for candidate in candidates:
        resolved = candidate.expanduser().resolve()
        if resolved.is_file() and os.access(resolved, os.X_OK):
            return resolved
    searched = ", ".join(str(candidate) for candidate in candidates)
    raise SmokeFailure(
        f"could not find an executable apl1 binary (searched: {searched}); "
        "build the project first or pass --app"
    )


def check_dependencies() -> None:
    missing = [tool for tool in ("Xvfb", "xterm", "xdotool") if shutil.which(tool) is None]
    if missing:
        raise SmokeFailure(
            "missing terminal-smoke dependencies: "
            + ", ".join(missing)
            + " (install Xvfb, xterm, and xdotool, or run with the TUI smoke test disabled)"
        )


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.timeout <= 0:
        print("smoke_tui.py: --timeout must be positive", file=sys.stderr)
        return 2

    smoke: Smoke | None = None
    failed = False
    try:
        check_dependencies()
        smoke = Smoke(args)
        smoke.app = find_app(args.app)
        smoke.log(f"app: {smoke.app}")
        smoke.log(f"timeout: {smoke.timeout:g}s")
        smoke.run_checks()
        return 0
    except SmokeFailure as error:
        failed = True
        if smoke is not None:
            smoke.log(f"FAILED: {error}")
        else:
            print(f"smoke_tui.py: FAILED: {error}", file=sys.stderr)
        return 1
    finally:
        if smoke is not None:
            smoke.stop_processes()
            output = smoke.write_output(failed=failed)
            if output is not None:
                print(f"[smoke] output: {output}", flush=True)
            if failed or args.keep:
                print(f"[smoke] temporary directory: {smoke.temp_dir}", flush=True)
            else:
                shutil.rmtree(smoke.temp_dir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
