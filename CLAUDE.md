# Working rules for this repo

These rules keep token use low. Follow them unless I say otherwise.

## Reading files
- Don't read a whole file when a search or a line range is enough. Use Grep to find the line, then Read with offset and limit.
- Don't re-read a file you already read in this session unless it changed.
- Don't read `PROGRESS.md`, `README.md` or the test files unless the task is about them.
- Skip generated and binary files: `__pycache__/`, `.pio/`, `*.pyc`, `*.kicad_sch` (large; use `firmware/tools/netcheck.py` or `check_board.py` instead).
- Use Explore or a subagent only for broad searches across many files. For one or two files, search directly.

## Making changes
- Make targeted edits with Edit. Don't rewrite a whole file with Write unless it is new or nearly all of it changes.
- Change only what the task needs. Don't reformat, rename or tidy code nearby.
- Don't re-read a file after editing it to check. Edit fails if the change doesn't apply.

## Running things
- Run tests with `bash firmware/run_tests.sh`. Don't run each test by hand unless one test is the focus.
- For firmware builds use `python -m platformio run -e calc1` (PlatformIO is not on the bash PATH). Build only the board you need.
- Pipe long output through `tail` or `grep` and keep only the lines that matter. Don't print full build logs.

## Replies
- Keep answers short. Lead with the result, then the evidence.
- Ask, don't guess. If a requirement, a file, or a hardware detail is unclear, ask me before acting on a guess.
- If you don't know something, say "I don't know" and say how you would find out. Don't present a guess as a fact.
- Check claims against the code or a command before stating them. Say which one you checked.
- Don't repeat file contents or command output back to me. Point to `file:line` instead.
- Ask before big changes, but not for small, obvious fixes.

## Git
- Work on a branch, not `main`. Commit and push only when I ask.
- Don't commit `firmware/README.txt` or other files I haven't reviewed without checking `git status` first.
- Keep commit messages short and in the style of the existing history.

## Scope
- Don't add tests, docs or features I didn't ask for. If you see a gap, mention it in one line and move on.
- Don't use subagents for work you can do with one or two tool calls.
