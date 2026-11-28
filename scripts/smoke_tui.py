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
        """Reconstruct the current terminal screen from the xterm log.

        Notcurses emits a cursor-addressed diff, so raw output is not a text
        transcript and unchanged cells are intentionally skipped. Applying the
        escape sequences to a character grid yields the actual visible screen,
        which makes assertions about the current state reliable.
        """
        raw = self.read_xterm_log()
        height = width = 400
        screen = [[" "] * width for _ in range(height)]
        row = column = 0
        saved = (0, 0)
        index = 0
        length = len(raw)
        while index < length:
            char = raw[index]
            if char == "\x1b":
                if index + 1 >= length:
                    break
                following = raw[index + 1]
                if following == "[":
                    cursor = index + 2
                    while (
                        cursor < length
                        and not ("@" <= raw[cursor] <= "~")
                    ):
                        cursor += 1
                    if cursor >= length:
                        break
                    params = raw[index + 2 : cursor]
                    final = raw[cursor]
                    index = cursor + 1
                    numbers = []
                    for part in params.lstrip("?<>=").split(";"):
                        try:
                            numbers.append(int(part))
                        except ValueError:
                            numbers.append(0)
                    if final in ("H", "f"):
                        row = max(0, (numbers[0] if numbers and numbers[0] else 1) - 1)
                        column = max(0, (numbers[1] if len(numbers) > 1 else 1) - 1)
                    elif final == "G":
                        column = max(0, (numbers[0] if numbers and numbers[0] else 1) - 1)
                    elif final == "d":
                        row = max(0, (numbers[0] if numbers and numbers[0] else 1) - 1)
                    elif final == "A":
                        row = max(0, row - (numbers[0] if numbers and numbers[0] else 1))
                    elif final == "B":
                        row = min(height - 1, row + (numbers[0] if numbers and numbers[0] else 1))
                    elif final == "C":
                        column = min(
                            width - 1, column + (numbers[0] if numbers and numbers[0] else 1)
                        )
                    elif final == "D":
                        column = max(0, column - (numbers[0] if numbers and numbers[0] else 1))
                    elif final == "J":
                        mode = numbers[0] if numbers else 0
                        if mode in (2, 3):
                            screen = [[" "] * width for _ in range(height)]
                        elif mode == 0:
                            for x in range(column, width):
                                screen[row][x] = " "
                            for y in range(row + 1, height):
                                screen[y] = [" "] * width
                    elif final == "K":
                        mode = numbers[0] if numbers else 0
                        if mode == 0:
                            for x in range(column, width):
                                screen[row][x] = " "
                        elif mode == 1:
                            for x in range(0, column + 1):
                                screen[row][x] = " "
                        elif mode == 2:
                            screen[row] = [" "] * width
                    elif final == "s":
                        saved = (row, column)
                    elif final == "u":
                        row, column = saved
                    continue
                if following == "(" and index + 2 < length:
                    index += 3
                    continue
                index += 2
                continue
            if char == "\r":
                column = 0
                index += 1
                continue
            if char == "\n":
                row = min(height - 1, row + 1)
                column = 0
                index += 1
                continue
            if char == "\b":
                column = max(0, column - 1)
                index += 1
                continue
            if char < " ":
                index += 1
                continue
            # Decode one UTF-8 code point and place it in one cell.
            step = 1
            byte = ord(char)
            if byte >= 0xF0:
                step = 4
            elif byte >= 0xE0:
                step = 3
            elif byte >= 0xC0:
                step = 2
            cell = raw[index : index + step]
            if 0 <= row < height and 0 <= column < width:
                screen[row][column] = cell
            column = min(width - 1, column + 1)
            index += step

        rendered = []
        for line in screen:
            text = "".join(line).rstrip()
            if text:
                rendered.append(text)
        return "\n".join(rendered)

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
    def start_xterm(self, geometry: str = "80x24", term: str = "xterm-256color") -> None:
        columns, rows = (int(part) for part in geometry.split("x"))
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
                "TERM": term,
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
        self.wait_for("xterm window", self._find_window, timeout=max(10.0, self.timeout))
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
        self.cell_width = width / columns
        self.cell_height = height / rows
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

        try:
            self.wait_for(description, present, timeout=10.0)
        except SmokeFailure as error:
            self.fail(f"{error}; visible screen: {self.visible_text()!r}")

    def send_keys(self, *keys: str) -> None:
        self.run(
            ["xdotool", "windowfocus", "--sync", self.window_id],
            env=self.display_env,
        )
        for key in keys:
            self.run(["xdotool", "key", key], env=self.display_env)
            time.sleep(0.08)

    def supports_terminal(self, term: str) -> bool:
        return (
            shutil.which("infocmp") is not None
            and subprocess.run(
                ["infocmp", term],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=False,
            ).returncode
            == 0
        )

    def terminal_rows(self) -> int:
        geometry = self.run(
            ["xdotool", "getwindowgeometry", "--shell", self.window_id],
            env=self.display_env,
        )
        height = 0
        for line in geometry.splitlines():
            if line.startswith("HEIGHT="):
                height = int(line.split("=", 1)[1])
        if height <= 0 or self.cell_height <= 0:
            self.fail(f"could not read xterm geometry: {geometry!r}")
        return max(1, int(height / self.cell_height))

    def terminal_columns(self) -> int:
        geometry = self.run(
            ["xdotool", "getwindowgeometry", "--shell", self.window_id],
            env=self.display_env,
        )
        width = 0
        for line in geometry.splitlines():
            if line.startswith("WIDTH="):
                width = int(line.split("=", 1)[1])
        if width <= 0 or self.cell_width <= 0:
            self.fail(f"could not read xterm geometry: {geometry!r}")
        return max(1, int(width / self.cell_width))

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

    def resize_and_check_redraw(
        self,
        needles: tuple[str, ...] = ("No teams have been added yet.",),
        wake_key: str = "Escape",
    ) -> None:
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
        self.send_keys(wake_key)
        self.wait_for(
            "output after resize",
            lambda: len(self.read_xterm_log()) > log_size_before,
            timeout=10.0,
        )
        self.wait_for_content("content after resize", needles)

    def quit(self) -> None:
        subprocess.run(
            ["xdotool", "windowfocus", "--sync", self.window_id],
            env=self.display_env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        self.send_keys("q")
        self.log("sent q to the application")
        self.wait_for_quit()

    def quit_via_button(self) -> None:
        columns = self.terminal_columns()
        self.click_cell(0, columns - 10)
        self.log("clicked the window-bar Quit button")
        self.wait_for_quit()

    def wait_for_quit(self) -> None:
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
        if version != 26:
            self.fail(f"database schema version is {version}, expected 26")

        with sqlite3.connect(self.database_path) as connection:
            teams = connection.execute(
                "SELECT name, archived FROM teams ORDER BY id"
            ).fetchall()
            active = connection.execute(
                "SELECT t.name FROM app_settings s "
                "JOIN teams t ON t.id = s.active_team_id WHERE s.id = 1"
            ).fetchone()
        if teams != [("Ferrari", 0), ("McLaren", 0), ("Red Bull", 0)]:
            self.fail(f"unexpected team rows after smoke run: {teams!r}")
        if active != ("Ferrari",):
            self.fail(f"active team after smoke run is {active!r}, expected Ferrari")

        with sqlite3.connect(self.database_path) as connection:
            drivers = connection.execute("SELECT name, short_name FROM drivers").fetchall()
            seasons = connection.execute("SELECT label, start_date, end_date FROM seasons").fetchall()
            cars = connection.execute("SELECT label, chassis FROM cars").fetchall()
            memberships = connection.execute(
                "SELECT start_date, end_date, role FROM team_driver_memberships"
            ).fetchall()
            ledger = connection.execute(
                "SELECT kind, amount_minor, currency, entry_date, category FROM ledger_entries ORDER BY id"
            ).fetchall()
            budgets = connection.execute(
                "SELECT category, amount_minor, currency, start_date, end_date FROM budget_plans ORDER BY id"
            ).fetchall()
            projects = connection.execute(
                "SELECT title, status, target_date FROM projects ORDER BY id"
            ).fetchall()
            project_updates = connection.execute(
                "SELECT update_date, summary, status_change FROM project_updates ORDER BY id"
            ).fetchall()
            part_types = connection.execute(
                "SELECT category, name FROM part_types ORDER BY id"
            ).fetchall()
            part_units = connection.execute(
                "SELECT serial_number, part_number FROM part_units ORDER BY id"
            ).fetchall()
            lifecycle = connection.execute(
                "SELECT action, event_date, position FROM lifecycle_events ORDER BY id"
            ).fetchall()
            team_currency = connection.execute(
                "SELECT currency FROM teams WHERE name = 'Ferrari'"
            ).fetchone()
            activity = connection.execute(
                "SELECT action FROM activity_events ORDER BY id"
            ).fetchall()
        if drivers != [("Charles Leclerc", "CL")]:
            self.fail(f"unexpected driver rows after smoke run: {drivers!r}")
        if seasons != [("2026", "2026-01-01", "2026-12-31")]:
            self.fail(f"unexpected season rows after smoke run: {seasons!r}")
        if cars != [("SF-26", "C1")]:
            self.fail(f"unexpected car rows after smoke run: {cars!r}")
        if memberships != [("2026-01-01", None, "Race seat")]:
            self.fail(f"unexpected membership rows after smoke run: {memberships!r}")
        if team_currency != ("EUR",):
            self.fail(f"unexpected team currency after smoke run: {team_currency!r}")
        if ledger != [
            ("opening_balance", 100000, "EUR", "2026-01-01", "Opening"),
            ("expense", -25000, "EUR", "2026-01-05", "Tyres"),
            ("reversal", 25000, "EUR", "2026-01-05", "Tyres"),
            ("adjustment", -20000, "EUR", "2026-01-05", "Tyres"),
        ]:
            self.fail(f"unexpected ledger rows after smoke run: {ledger!r}")
        if budgets != [("Tyres", 30000, "EUR", "2026-01-01", "2026-01-31")]:
            self.fail(f"unexpected budget rows after smoke run: {budgets!r}")
        if projects != [("Aero", "", None)]:
            self.fail(f"unexpected project rows after smoke run: {projects!r}")
        if project_updates != [("2026-01-10", "First update", "")]:
            self.fail(f"unexpected project update rows after smoke run: {project_updates!r}")
        if part_types != [("Aero", "Front Wing")]:
            self.fail(f"unexpected part type rows after smoke run: {part_types!r}")
        if part_units != [("", "")]:
            self.fail(f"unexpected part unit rows after smoke run: {part_units!r}")
        if lifecycle != [("install", "2026-02-01", "front")]:
            self.fail(f"unexpected lifecycle rows after smoke run: {lifecycle!r}")
        expected_activity = [
            ("team_created",),
            ("team_created",),
            ("team_created",),
            ("driver_created",),
            ("membership_added",),
            ("season_created",),
            ("car_created",),
            ("team_currency_updated",),
            ("ledger_opening_balance",),
            ("ledger_expense",),
            ("budget_created",),
            ("ledger_corrected",),
            ("project_created",),
            ("project_update_logged",),
            ("part_type_created",),
            ("part_unit_created",),
            ("lifecycle_installed",),
        ]
        if activity != expected_activity:
            self.fail(f"unexpected activity rows after smoke run: {activity!r}")

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
        self.wait_for_content(
            "mouse tab switch",
            ("No active team. Select one, or press A for an explicit all-teams view.",),
        )
        self.click_cell(2, 3)
        self.wait_for_content("tab return to Teams", ("No teams have been added yet.",))
        self.send_keys("Escape")
        self.send_keys("comma")
        self.wait_for_content("comma previous view", ("Overview is not implemented in this phase.",))
        self.send_keys("period")
        self.wait_for_content("period next view", ("No teams have been added yet.",))
        self.log("switched tabs by mouse and by the ,/. keys")
        self.send_keys("m")
        self.wait_for_content("keyboard menu dropdown", ("Active team overview",))
        self.send_keys("Escape", "Escape")
        self.log("opened and closed the window menu by keyboard")
        self.click_cell(0, 3)
        self.wait_for_content("mouse menu dropdown", ("Active team overview",))
        self.send_keys("Escape", "Escape")
        self.log("opened and closed the window menu by mouse")

        columns = self.terminal_columns()
        self.click_cell(0, columns - 3)
        self.wait_for_content("window-bar keys button", ("Keyboard and mouse help",))
        self.send_keys("Escape")
        self.log("opened contextual keyboard help from the window bar")

        self.send_keys("m", "Right", "Right", "Return")
        self.wait_for_content("database information dialog", ("Database information",))
        dialog_screen = self.visible_text()
        if dialog_screen.count("| Database information") != 1:
            self.fail("database information title was rendered more than once")
        if dialog_screen.count("The SQLite database is chosen at launch with") != 1:
            self.fail("database information message first line was duplicated")
        self.send_keys("Escape")
        self.send_keys("Escape")
        self.log("database information dialog rendered its first line once")

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
        self.wait_for_content(
            "status bar team and view context",
            ("Ferrari \u00b7 Teams", ": commands"),
        )
        self.log("created Ferrari through the form")

        self.send_keys("Escape", "n")
        self.type_text("McLaren")
        self.send_keys("Return")
        self.wait_for_database(
            "second team row",
            lambda: self.database_query("SELECT COUNT(*) FROM teams;") == (2,),
        )
        self.log("created McLaren through the form")

        self.send_keys("t")
        self.wait_for_content("keyboard team picker", ("Switch active team", "McLaren"))
        self.send_keys("Up", "Return")
        self.wait_for_database(
            "switched active team",
            lambda: self.database_query(
                "SELECT t.name FROM app_settings s "
                "JOIN teams t ON t.id = s.active_team_id WHERE s.id = 1;"
            )
            == ("Ferrari",),
        )
        self.wait_for_content("rail and status active team", ("Team: Ferrari", "Ferrari · Teams"))
        self.log("switched active team to Ferrari with the team picker")

        rows = self.terminal_rows()
        self.click_cell(rows - 2, 3)
        self.wait_for_content("mouse rail team picker", ("Switch active team",))
        self.send_keys("Escape")
        self.click_cell(rows - 1, 2)
        self.wait_for_content("mouse status team picker", ("Switch active team",))
        self.send_keys("Escape")
        self.log("opened the team picker from the rail and status bar by mouse")

        # Command-line UI: cancel does nothing, a navigation command switches
        # views, incomplete action commands open prefilled dialogs, and complete
        # commands execute once through the same use cases.
        self.type_text(":")
        self.type_text("view money")
        self.send_keys("Escape")
        if "Money: Ferrari" in self.visible_text():
            self.fail("cancelling a command still changed the view")
        self.log("command cancel left the current view unchanged")

        self.type_text(":")
        self.type_text("view money")
        self.send_keys("Return")
        self.wait_for_content("command view switch", ("Money: Ferrari", "No reporting currency"))
        self.type_text(":")
        self.type_text("view t")
        self.wait_for_content("view argument completion", ("view teams",))
        self.send_keys("Tab")
        self.send_keys("Return")
        self.wait_for_content("completed view applies", ("Ferrari", "McLaren"))
        self.type_text(":")
        self.type_text("money")
        self.send_keys("Return")
        self.wait_for_content("direct view shortcut", ("Money: Ferrari", "No reporting currency"))
        self.type_text(":")
        self.type_text("tl")
        self.send_keys("Return")
        self.wait_for_content("command return to teams", ("Ferrari", "McLaren"))
        self.log("view completion, direct :money shortcut, and :tl applied their views")

        self.type_text(":")
        self.type_text("team create")
        self.send_keys("Return")
        self.wait_for_content("incomplete command form", ("Create team", "Name:"))
        self.send_keys("Escape")
        if self.database_query("SELECT COUNT(*) FROM teams;") != (2,):
            self.fail("cancelling the incomplete team command wrote a row")
        self.log("incomplete command opened the prefilled team form and cancelled cleanly")

        self.type_text(":")
        self.type_text('team create --name "Red Bull" --notes "Energy"')
        self.send_keys("Return")
        self.wait_for_database(
            "command-created team",
            lambda: self.database_query("SELECT COUNT(*) FROM teams;") == (3,),
        )
        self.wait_for_content("command-created detail", ("Name: Red Bull",))
        self.type_text(":")
        self.type_text("team switch Ferrari")
        self.send_keys("Return")
        self.wait_for_database(
            "command active team",
            lambda: self.database_query(
                "SELECT t.name FROM app_settings s "
                "JOIN teams t ON t.id = s.active_team_id WHERE s.id = 1;"
            )
            == ("Ferrari",),
        )
        self.send_keys("Escape")
        self.log("complete command created a team and switched teams")

        # Phase 3 reference records: driver, dated membership, season, car, and
        # the team-scoped activity history. Commands and dialogs use the same
        # validated application services.
        self.type_text(":")
        self.type_text('driver new --name "Charles Leclerc" --short CL')
        self.send_keys("Return")
        self.wait_for_content("driver command detail", ("Name: Charles Leclerc",))
        rows = self.terminal_rows()
        self.click_cell(rows - 2, 30)
        self.wait_for_content("membership form from mouse key hint", ("Add team membership", "Start date"))
        self.type_text("2026-01-01")
        self.send_keys("Tab", "Tab")
        self.type_text("Race seat")
        self.send_keys("Return")
        self.wait_for_database(
            "membership row",
            lambda: self.database_query("SELECT COUNT(*) FROM team_driver_memberships;")
            == (1,),
        )
        self.wait_for_content(
            "person detail membership",
            ("Name: Charles Leclerc", "2026-01-01 to open", "Race seat"),
        )
        self.log("created a driver and dated membership through the form")

        self.type_text(":")
        self.type_text("season new --label 2026 --start 2026-01-01 --end 2026-12-31")
        self.send_keys("Return")
        self.wait_for_content("season command detail", ("Label: 2026", "2026-12-31"))
        self.type_text(":")
        self.type_text("car new --label SF-26 --chassis C1")
        self.send_keys("Return")
        self.wait_for_content("car command detail", ("Identifier: SF-26", "Chassis: C1"))
        self.type_text(":")
        self.type_text("history")
        self.send_keys("Return")
        self.wait_for_content(
            "history view",
            ("History: active team", "team_created", "driver_created", "Created team 'Ferrari'"),
        )
        self.click_cell(2, 36)
        self.wait_for_content("history entity filter changed by mouse", ("Ent:team",))
        self.click_cell(2, 45)
        self.wait_for_content("history action filter changed by mouse", ("Act:team:created",))
        self.click_cell(2, 74)
        self.wait_for_content(
            "history filters cleared by mouse", ("Ent:all", "Act:all")
        )
        self.click_cell(4, 25)
        self.wait_for_content("history row mouse detail", ("Activity detail", "car_created"))
        self.send_keys("Escape")
        self.log("history filters and first row responded to mouse")
        self.type_text(":")
        self.type_text('driver show "Charles Leclerc"')
        self.send_keys("Return")
        self.wait_for_content("driver show command", ("Name: Charles Leclerc",))
        self.type_text(":")
        self.type_text("tl")
        self.send_keys("Return")
        self.wait_for_content("return to team list", ("Ferrari", "McLaren", "Red Bull"))
        self.click_cell(1, 25)
        self.wait_for_content("team detail opened by mouse", ("Name: Ferrari", "Related records"))
        self.send_keys("Escape")
        self.log("season, car, history, and driver-show command routes applied")

        # Phase 4: choose a currency, post an opening balance and expense,
        # compare a plan with actuals, and correct the expense traceably.
        self.type_text(":")
        self.type_text("money")
        self.send_keys("Return")
        self.wait_for_content("money view before currency", ("Money: Ferrari", "No reporting currency"))
        self.click_cell(2, 24)
        self.wait_for_content("money date filter clicked", ("Date:30d",))
        self.click_cell(2, 58)
        self.wait_for_content("money filters cleared by mouse", ("Date:all", "Kind:all", "Category:all"))
        self.send_keys("u")
        self.wait_for_content("currency form", ("Reporting currency", "Currency"))
        self.type_text("EUR")
        self.send_keys("Return")
        self.wait_for_content("currency set", ("Currency: EUR",))

        self.send_keys("o")
        self.wait_for_content("opening form", ("Record opening balance", "Amount"))
        self.type_text("1000.00")
        self.send_keys("Tab")
        self.type_text("2026-01-01")
        self.send_keys("Tab")
        self.type_text("Opening")
        self.send_keys("Tab")
        self.type_text("Opening balance")
        self.send_keys("Return")
        self.wait_for_content("opening review", ("Review money entry", "EUR", "Opening"))
        self.send_keys("Return")
        self.wait_for_database(
            "opening balance row",
            lambda: self.database_query("SELECT COUNT(*) FROM ledger_entries;") == (1,),
        )
        self.wait_for_content("opening detail", ("Opening balance", "EUR 1000.00"))
        self.send_keys("Escape")

        self.send_keys("x")
        self.wait_for_content("expense form", ("Record expense", "Amount"))
        self.type_text("250.00")
        self.send_keys("Tab")
        self.type_text("2026-01-05")
        self.send_keys("Tab")
        self.type_text("Tyres")
        self.send_keys("Tab")
        self.type_text("Test tyres")
        self.send_keys("Return")
        self.wait_for_content("expense review", ("Review money entry", "EUR -250.00"))
        self.send_keys("Return")
        self.wait_for_database(
            "expense row",
            lambda: self.database_query("SELECT COUNT(*) FROM ledger_entries;") == (2,),
        )
        self.wait_for_content("expense detail", ("Expense", "EUR -250.00", "Entry ID: #2"))
        self.send_keys("Escape")

        self.send_keys("b")
        self.wait_for_content("budget report", ("Budget report:", "unplanned"))
        self.send_keys("n")
        self.wait_for_content("budget form", ("Create budget plan", "Start date"))
        self.type_text("2026-01-01")
        self.send_keys("Tab")
        self.type_text("2026-01-31")
        self.send_keys("Tab", "Tab")
        self.type_text("300.00")
        self.send_keys("Tab", "Tab")
        self.type_text("January")
        self.send_keys("Return")
        self.wait_for_content("budget review", ("Review budget plan", "EUR", "Tyres"))
        self.send_keys("Return")
        self.wait_for_database(
            "budget plan row",
            lambda: self.database_query("SELECT COUNT(*) FROM budget_plans;") == (1,),
        )
        self.wait_for_content("budget comparison", ("Budget report:", "Tyres", "EUR 300.00", "EUR 250.00"))
        self.send_keys("Escape")

        self.send_keys("Return")
        self.wait_for_content("expense detail for correction", ("Activity:",))
        self.send_keys("c")
        self.wait_for_content("correction form", ("Correct money entry", "New amount"))
        self.type_text("200.00")
        self.send_keys("Tab", "Tab", "Tab")
        self.type_text("Corrected tyre price")
        self.send_keys("Return")
        self.wait_for_content("correction review", ("Review correction", "200.00"))
        self.send_keys("Return")
        self.wait_for_database(
            "correction rows",
            lambda: self.database_query("SELECT COUNT(*) FROM ledger_entries;") == (4,),
        )
        self.wait_for_content("correction detail", ("Adjustment", "EUR -200.00"))
        self.send_keys("Escape")
        self.send_keys("b")
        self.wait_for_content("corrected budget comparison", ("Budget report:", "EUR 200.00"))
        self.send_keys("Escape")
        self.type_text(":")
        self.type_text("tl")
        self.send_keys("Return")
        self.wait_for_content("return to teams after money", ("Ferrari", "McLaren", "Red Bull"))
        self.log("currency, opening balance, expense, budget, and correction flows persisted")

        # Phase 5: the Development view lists active-team projects, creates
        # one through the shared form path, and logs a dated update.
        self.type_text(":")
        self.type_text("view development")
        self.send_keys("Return")
        self.wait_for_content("development view", ("Development:", "No projects match"))
        self.send_keys("n")
        self.wait_for_content("project form", ("Create project", "Title"))
        self.type_text("Aero")
        self.send_keys("Tab", "Tab", "Tab", "Tab", "Tab", "Tab", "Tab", "Tab")
        self.send_keys("Return")
        self.wait_for_database(
            "project row",
            lambda: self.database_query("SELECT COUNT(*) FROM projects;") == (1,),
        )
        self.wait_for_content("project detail", ("Project: Aero", "No updates recorded"))
        self.send_keys("u")
        self.wait_for_content("project update form", ("Log project update", "Update date"))
        self.type_text("2026-01-10")
        self.send_keys("Tab")
        self.type_text("First update")
        self.send_keys("Tab", "Tab", "Tab")
        self.send_keys("Return")
        self.wait_for_database(
            "project update row",
            lambda: self.database_query("SELECT COUNT(*) FROM project_updates;") == (1,),
        )
        self.wait_for_content("project timeline", ("First update",))
        self.send_keys("Escape")
        self.wait_for_content("project list after detail", ("Aero",))
        self.type_text(":")
        self.type_text("tl")
        self.send_keys("Return")
        self.wait_for_content("return to teams after development", ("Ferrari", "McLaren"))
        self.log("created a project and logged a dated update through the forms")

        # Phase 6: create a user-defined part type and unit, then review and
        # record an install. This verifies the register is reachable from Parts
        # and that lifecycle changes refresh derived fitment.
        self.type_text(":")
        self.type_text("view parts")
        self.send_keys("Return")
        self.wait_for_content("parts fitment view", ("Recorded fitment slots",))
        self.click_cell(4, 40)
        self.send_keys("y")
        self.wait_for_content("part type register", ("Part types:", "No part types yet"))
        self.send_keys("n")
        self.wait_for_content("part type form", ("Create part type", "Category", "Name"))
        self.type_text("Aero")
        self.send_keys("Tab")
        self.type_text("Front Wing")
        self.send_keys("Return")
        self.wait_for_content("created part type", ("Part types: 1", "Front Wing", "Aero"))
        if self.database_query("SELECT COUNT(*) FROM part_types;") != (1,):
            self.fail("part type form did not persist exactly one type")

        self.send_keys("Return")
        self.wait_for_content("part type detail", ("Part type #", "Front Wing", "Aero"))
        self.send_keys("n")
        self.wait_for_content("part unit form", ("Create part unit", "Type ID"))
        self.send_keys("Return")
        self.wait_for_content("part unit detail", ("Part unit #", "Front Wing", "Current fitment"))
        if self.database_query("SELECT COUNT(*) FROM part_units;") != (1,):
            self.fail("part unit form did not persist exactly one unit")

        self.send_keys("i")
        self.wait_for_content("install form", ("Install", "Unit ID", "Date"))
        self.send_keys("Tab")
        self.type_text("SF-26")
        self.send_keys("Tab")
        self.type_text("front")
        self.send_keys("Tab")
        self.type_text("2026-02-01")
        self.send_keys("Return")
        self.wait_for_content("install review", ("Review lifecycle action", "Install unit", "Confirm"))
        self.send_keys("Tab", "Return")
        self.wait_for_database(
            "lifecycle install row",
            lambda: self.database_query(
                "SELECT COUNT(*) FROM lifecycle_events WHERE action = 'install';"
            )
            == (1,),
        )
        self.send_keys("Escape", "Escape")
        self.wait_for_content("derived fitment row", ("Recorded fitment slots: 1", "SF-26", "front"))
        self.log("created a part register entry and recorded an install through its review form")
        self.type_text(":")
        self.type_text("tl")
        self.send_keys("Return")
        self.wait_for_content("return to teams after parts", ("Ferrari", "McLaren"))

        self.type_text(":")
        self.type_text("team switch Missing")
        self.send_keys("Return")
        self.wait_for_content("unknown team error", ("Team not found",))
        self.send_keys("Escape")
        if self.database_query(
            "SELECT t.name FROM app_settings s "
            "JOIN teams t ON t.id = s.active_team_id WHERE s.id = 1;"
        ) != ("Ferrari",):
            self.fail("a failed team switch changed the active team")
        self.log("failed team command left the active team unchanged")

        self.type_text(":")
        self.type_text("version")
        self.send_keys("Return")
        self.wait_for_content("version command", ("0.1.0",))
        self.send_keys("Escape")

        self.type_text(":")
        self.type_text("help")
        self.send_keys("Return")
        self.wait_for_content("formatted command help", ("Command help", "Views", "Teams", "(tl)", "Shortcuts:"))
        help_screen = self.visible_text()
        if "- team list (tl)" not in help_screen:
            self.fail("command help lost its explicit line breaks")
        self.send_keys("Escape")
        self.log("command help kept headings and one command per line")

        self.type_text(":")
        self.type_text("team create")
        self.send_keys("Return")
        self.wait_for_content("incomplete command form", ("Create team",))
        self.send_keys("Escape")
        self.send_keys("n")
        self.type_text("Ferrari")
        self.send_keys("Return")
        self.wait_for_content("duplicate validation", ("already exists",))
        if self.database_query("SELECT COUNT(*) FROM teams;") != (3,):
            self.fail("duplicate team validation wrote a row")
        self.send_keys("Escape")
        self.log("duplicate team names keep the form open without writing")

        rows = self.terminal_rows()
        columns = self.terminal_columns()
        self.click_cell(rows - 1, columns - 3)
        self.wait_for_content("mouse command prompt", (":|   view",))
        self.send_keys("Escape")
        self.log("opened the command prompt from the status hint by mouse")

        self.send_keys("m")
        self.resize_and_check_redraw(("Database information",), wake_key="F10")
        self.send_keys("Escape", "Escape")
        self.send_keys("t")
        self.resize_and_check_redraw(("Switch active team",), wake_key="F10")
        self.send_keys("Escape")
        self.log("resize kept the open menu and team dialog usable")

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

        if self.supports_terminal("xterm-mono"):
            self.start_xterm(term="xterm-mono")
            self.wait_for_content(
                "limited-color startup",
                ("Overview", "Teams", "Ferrari", ": commands"),
            )
            self.quit_via_button()
            self.check_exit_status()
            self.check_window_cleanup()
            self.log("limited-color terminal remained fully operable")
        else:
            self.log("xterm-mono terminfo unavailable; limited-color launch skipped")

        self.start_xterm(geometry="40x10")
        self.wait_for_content("compact startup", ("Overview", "Teams", "Parts"))
        self.send_keys("n")
        self.wait_for_content("compact dialog", ("Create team", "Name:", "Notes", "[ Save ]"))
        compact_screen = self.visible_text()
        if compact_screen.count("Create team") != 1:
            self.fail("compact dialog rendered its title more than once")
        self.send_keys("Escape")
        self.quit()
        self.check_exit_status()
        self.check_window_cleanup()
        self.log("compact 40x10 dialog rendered once and cancelled cleanly")

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
