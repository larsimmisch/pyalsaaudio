.PHONY: lint test

lint:
	uv run --group lint pyright
	uv run --group lint stubtest

test:
	uv run tests/test.py
