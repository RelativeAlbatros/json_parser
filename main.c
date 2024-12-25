#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

	int c = input[lexer_index];
	int d = strlen(input);
	while (lexer_index < strlen(input)) {
		switch (c) {
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
				lexer_index += 3;
				return (Token){TOKEN_TRUE, strdup("true")};
			} else if (!strncmp(buffer, "false", 5)) {
				lexer_index += 4;
				return (Token){TOKEN_FALSE, strdup("false")};
			} else if (!strncmp(buffer, "null", 4)) {
				lexer_index += 3;
				return (Token){TOKEN_NULL, strdup("null")};
			} else if (isdigit(c) || c == '-') {
				return parse_number(input);
			} else if (c == ' ' || c == '\n') {
				lexer_index++;
				continue;
			} else {
				return (Token){TOKEN_ERR, NULL};
			}
		}
	}
}

void free_token(Token* token) {
	if (token->value) {
		free(token->value);
	}
}

int main(int argc, char** argv) {
	FILE* fp = fopen("test.json", "r");
	if (fp == NULL) {
		fprintf(stderr, "Error: opening file\n");
		return -1;
	}
	char input[256];
	Token token;

	while ((fgets(input, 256, fp)) != NULL) {
		while ((token = get_next_token(input)).type != TOKEN_EOF) {
			if (token.type == TOKEN_ERR) {
				fprintf(stderr, "Error: %s\n", token.value);
				free_token(&token);
				break;
			} else if (token.type == TOKEN_EOF) {
				break;
			}
			printf("Token type: %d, value: %s\n", token.type, token.value);
			free_token(&token);
		}
	}

	fclose(fp);
	return 0;
}
