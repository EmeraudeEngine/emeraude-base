#!/usr/bin/env python3
#
# src/Testing/coverage-ratchet.py — the coverage ratchet of emeraude-base (Ave Robustus II, gap 4, 2026-10-18).
#
# Reads an `llvm-cov report -show-branch-summary` text report and compares each file's line and branch coverage with
# the recorded baseline (src/Testing/coverage-baseline.tsv). Any file that drops below its baseline by more than the
# tolerance fails the check (exit 1): coverage only goes up. The tolerance is the larger of a number of percentage
# points and TWO units of the file (two lines, two branches): the parallel code paths take a branch or two more or
# less from one run to the next (ThreadPool.hpp: 1 branch of 70, 1.43 points, with no change). A file of the report absent from the baseline is listed
# (a new file: add it with --write-baseline); a file of the baseline absent from the report is listed too (renamed or
# removed: update the baseline on purpose).
#
# Usage:   src/Testing/coverage-ratchet.py <report.txt>                  check against the baseline
#          src/Testing/coverage-ratchet.py <report.txt> --write-baseline  record the report as the new baseline
# The report comes from the recipe of projet-alpha docs/unit-testing.md § Coverage (exclude _deps and Testing/).

import argparse
import os
import sys

BASELINE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "coverage-baseline.tsv")

# A measure moves a little with the thread interleavings of the parallel code paths: a drop within this many
# percentage points, or within SLACK_UNITS lines / branches of the file, is not a regression.
DEFAULT_TOLERANCE = 1.0
SLACK_UNITS = 2


def parse_percent(text):
	"""Returns a percentage as a float, or None for "-" (no branch in the file)."""
	if text == "-":
		return None
	return float(text.rstrip("%"))


def read_report(path):
	"""Returns {file: (lines %, lines total, branches %, branches total)} from an llvm-cov report with the branch summary."""
	coverage = {}

	with open(path, encoding="utf-8") as report:
		for line in report:
			fields = line.split()

			# Filename, regions (3 columns), functions (3), lines (3), branches (3).
			if len(fields) != 13 or fields[0] in ("Filename", "TOTAL") or not fields[0][0].isalnum():
				continue

			coverage[fields[0]] = (parse_percent(fields[9]), int(fields[7]), parse_percent(fields[12]), int(fields[10]))

	return coverage


def read_baseline():
	baseline = {}

	with open(BASELINE, encoding="utf-8") as table:
		for line in table:
			if not line.strip() or line.startswith("#"):
				continue

			name, lines, linesTotal, branches, branchesTotal = line.rstrip("\n").split("\t")
			baseline[name] = (parse_percent(lines), int(linesTotal), parse_percent(branches), int(branchesTotal))

	return baseline


def format_percent(value):
	return "-" if value is None else f"{value:.2f}%"


def write_baseline(coverage):
	with open(BASELINE, "w", encoding="utf-8") as table:
		table.write("# emeraude-base coverage baseline: file, lines %, lines, branches %, branches (coverage-ratchet.py).\n")

		for name in sorted(coverage):
			lines, linesTotal, branches, branchesTotal = coverage[name]
			table.write(f"{name}\t{format_percent(lines)}\t{linesTotal}\t{format_percent(branches)}\t{branchesTotal}\n")

	print(f"baseline written: {len(coverage)} files -> {BASELINE}")


def check(coverage, tolerance):
	baseline = read_baseline()
	drops = []

	for name, (baseLines, _, baseBranches, _) in sorted(baseline.items()):
		if name not in coverage:
			print(f"missing from the report (renamed or removed?): {name}")
			continue

		lines, linesTotal, branches, branchesTotal = coverage[name]

		for label, before, after, total in (("lines", baseLines, lines, linesTotal), ("branches", baseBranches, branches, branchesTotal)):
			if before is None or after is None or total == 0:
				continue

			allowed = max(tolerance, 100.0 * SLACK_UNITS / total)

			if after < before - allowed:
				drops.append(f"{name}: {label} {format_percent(before)} -> {format_percent(after)} (allowed {allowed:.2f} points)")

	for name in sorted(set(coverage) - set(baseline)):
		print(f"new file, not in the baseline: {name} (lines {format_percent(coverage[name][0])})")

	if drops:
		print(f"COVERAGE DROPPED ({len(drops)}):")

		for drop in drops:
			print(f"  {drop}")

		return 1

	print(f"coverage ratchet OK: {len(baseline)} files, none below its baseline (tolerance {tolerance} points or {SLACK_UNITS} units)")

	return 0


def main():
	parser = argparse.ArgumentParser(description="The coverage ratchet of emeraude-base.")
	parser.add_argument("report", help="an llvm-cov report -show-branch-summary text file")
	parser.add_argument("--write-baseline", action="store_true", help="record the report as the new baseline")
	parser.add_argument("--tolerance", type=float, default=DEFAULT_TOLERANCE, help="the allowed drop in percentage points (at least two units of the file)")
	arguments = parser.parse_args()

	coverage = read_report(arguments.report)

	if not coverage:
		print(f"no file in {arguments.report}: is it an llvm-cov report with -show-branch-summary?", file=sys.stderr)
		return 2

	if arguments.write_baseline:
		write_baseline(coverage)
		return 0

	return check(coverage, arguments.tolerance)


if __name__ == "__main__":
	sys.exit(main())
