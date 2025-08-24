.PHONY: lint test

lint:
	uv run pyright
	uv run stubtest

test:
	uv run tests/test.py
