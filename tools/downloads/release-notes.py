#!/usr/bin/env python3
"""The release notes the updaters show: the newest release in CHANGELOG.md
(read from stdin), as a plain HTML fragment for an update feed's
<description>.  The newest is the release's own once the release
steps have moved Unreleased under its version; before that, Unreleased.

    git show <commit>:CHANGELOG.md | python3 -I release-notes.py

Handles what CHANGELOG.md uses: ### headings, "- " items with indented
continuation lines, paragraphs, **bold**, `code` and [links](...) (kept as
their text: the updater's window is no place to browse).
"""

import html
import re
import sys

STYLE = (
    "<style>"
    "body{font:13px/1.45 system-ui,'Segoe UI',-apple-system,sans-serif;margin:8px}"
    "h3{font-size:13px;margin:12px 0 4px}"
    "ul{margin:0;padding-left:18px}li{margin:3px 0}p{margin:6px 0}"
    "</style>"
)


def inline(text):
    text = html.escape(text, quote=False)
    text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", text)
    return re.sub(r"`([^`]+)`", r"<code>\1</code>", text)


def newest_section(lines):
    section, inside = [], False
    for line in lines:
        if line.startswith("## "):
            if inside:
                break
            # A release ("## 0.8.1, ...") or Unreleased, not "## Versions".
            inside = re.match(r"## (Unreleased|\d)", line) is not None
        elif inside:
            section.append(line.rstrip("\n"))
    return section


def blocks(section):
    """(kind, text) for each heading, list item and paragraph."""
    current = None
    for line in section:
        if not line.strip():
            if current:
                yield current
            current = None
        elif line.startswith("### "):
            if current:
                yield current
            yield ("h3", line[4:].strip())
            current = None
        elif line.startswith("- "):
            if current:
                yield current
            current = ("li", line[2:].strip())
        elif current:
            current = (current[0], current[1] + " " + line.strip())
        else:
            current = ("p", line.strip())
    if current:
        yield current


def main():
    out, in_list = [STYLE], False
    for kind, text in blocks(newest_section(sys.stdin)):
        if kind == "li" and not in_list:
            out.append("<ul>")
        elif kind != "li" and in_list:
            out.append("</ul>")
        in_list = kind == "li"
        out.append(f"<{kind}>{inline(text)}</{kind}>")
    if in_list:
        out.append("</ul>")
    if len(out) == 1:
        sys.exit("CHANGELOG.md has no release section")
    print("\n".join(out))


main()
