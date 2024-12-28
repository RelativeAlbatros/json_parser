VERSION:="0.1"

debug:
	gcc main.c -g -Wall -Wextra -pedantic -o json_parser_${VERSION}

test: debug
	json_parser_${VERSION} tests/step1/valid.json
