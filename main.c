#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BUF_SIZE 256

static int lexer_index = 0;

typedef enum {
	TOKEN_STRING,
	TOKEN_CURL_OPEN,
	TOKEN_CURL_CLOSE,
	TOKEN_BRACKET_OPEN,
	TOKEN_BRACKET_CLOSE,
	TOKEN_COMMA,
	TOKEN_COLON,
	TOKEN_NUMBER,
	TOKEN_TRUE,
	TOKEN_FALSE,
	TOKEN_NULL,
	TOKEN_NL,
	TOKEN_ERR,
	TOKEN_EOF
} TokenType;

typedef struct {
	TokenType type;
	char* value;
} Token;

Token parse_string(const char* input);
Token parse_number(const char* input);
Token get_next_token(const char* input);
void free_token(Token* token);

Token parse_string(const char* input) {
	char* string_value = strdup("");
	while (input[lexer_index++] != '\"') {
		// append on the fly const char* from char to string_value
		strcat(string_value, (char[2]){input[lexer_index-1], '\0'}); 
	}

	return (Token){TOKEN_STRING, string_value};
}

Token parse_number(const char* input) {
	int isnegative = (input[lexer_index]=='-') ? 1 : 0;
	char* num_value = strdup("");

	lexer_index += isnegative;
	while (isdigit(input[lexer_index++])) {
		strcat(num_value, (char[2]){input[lexer_index-1], '\0'});
	}

	return (Token){TOKEN_NUMBER, num_value};
}

Token get_next_token(const char* input) {
	char buffer[5];
	int c;

	if (lexer_index >= strlen(input)) {
		return (Token){TOKEN_EOF, NULL};
	}
	// skip spaces and tabs
	while (input[lexer_index] == ' ' || input[lexer_index] == '\t') {
		lexer_index++;
	}
	switch (c = input[lexer_index]) {
	case '\"':
		lexer_index++;
		return parse_string(input);
	case '{':
		lexer_index++;
		return (Token){TOKEN_CURL_OPEN, strdup("{")};
	case '}':
		lexer_index++;
		return (Token){TOKEN_CURL_CLOSE, strdup("}")};
	case '[':
		lexer_index++;
		return (Token){TOKEN_BRACKET_OPEN, strdup("[")};
	case ']':
		lexer_index++;
		return (Token){TOKEN_BRACKET_CLOSE, strdup("]")};
	case ',':
		lexer_index++;
		return (Token){TOKEN_COMMA, strdup(",")};
	case ':':
		lexer_index++;
		return (Token){TOKEN_COLON, strdup(":")};
	case '\0':
		lexer_index++;
		return (Token){TOKEN_EOF, NULL};
	default:
		strncpy(buffer, input+lexer_index, 5);
		if (!strncmp(buffer, "true", 4)) {
			lexer_index += 4;
			return (Token){TOKEN_TRUE, strdup("true")};
		} else if (!strncmp(buffer, "false", 5)) {
			lexer_index += 5;
			return (Token){TOKEN_FALSE, strdup("false")};
		} else if (!strncmp(buffer, "null", 4)) {
			lexer_index += 4;
			return (Token){TOKEN_NULL, strdup("null")};
		} else if (isdigit(c) || c == '-') {
			return parse_number(input);
		} else if (c == '\n') {
			return (Token){TOKEN_NL, NULL};
		} else {
			return (Token){TOKEN_ERR, NULL};
		}
	}
}

void free_token(Token* token) {
	if (token->value) {
		free(token->value);
	}
}

int main(int argc, char** argv) {
	FILE* fp;
	
	if (argc == 1) {
		fprintf(stderr, "Error: which file?\n");
		exit(-1);
	} else {
		fp = fopen(argv[1], "r");
		if (fp == NULL) {
			fprintf(stderr, "Error: opening file\n");
			exit(-1);
		}
	}
	char* input = malloc(BUF_SIZE);
	Token token = {TOKEN_NULL, NULL};

	while ((fgets(input, BUF_SIZE, fp)) != NULL) {
		lexer_index = 0;
		if (input[strlen(input) - 1] != '\n' && !feof(fp)) {
			fprintf(stderr, "Error: Line too long for buffer.\n");
		}
		while (((token = get_next_token(input)).type != TOKEN_NL) && token.type != TOKEN_EOF) {
			if (token.type == TOKEN_ERR) {
				fprintf(stderr, "Error: %s\n", token.value);
				free_token(&token);
				break;
			} else if (token.type == TOKEN_EOF) {
				break;
			}
			printf("Token type: %d", token.type);
			if (token.value) {
				printf(" value: %s\n", token.value);
			} else {
				printf("\n");
			}
		}
		free_token(&token);
	}

	free(input);
	fclose(fp);
	return 0;
}
