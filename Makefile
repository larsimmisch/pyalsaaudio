.PHONY: lint test build_wheels build_sdist build

lint:
	uv run --group lint pyright; \
	uv run --group lint stubtest alsaaudio

test:
	uv run tests/test.py

build_wheels:
	uvx cibuildwheel==3.1.4

build_sdist:
	uv build --sdist

build: build_sdist build_wheels
