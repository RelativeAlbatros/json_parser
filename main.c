#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>

#define BUF_SIZE 256
#define OBJ_INIT_CAP 24

static size_t lexer_index = 0;

typedef enum {
	TOKEN_CURL_OPEN,
	TOKEN_CURL_CLOSE,
	TOKEN_BRACKET_OPEN,
	TOKEN_BRACKET_CLOSE,
	TOKEN_COMMA,
	TOKEN_COLON,
	TOKEN_STRING,
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

typedef struct {
	Token* data;
	size_t size; // number of elements
	size_t capacity; // allocated memory
} TokenArray;

void free_token(Token* token);
TokenArray* initTokenArray(size_t initial_capacity);
void addToken(TokenArray* arr, Token new_token);
Token* getToken(TokenArray* arr, size_t index);
void removeToken(TokenArray* arr, size_t index);
void freeTokenArray(TokenArray* arr);
Token parse_string(const char* input);
Token parse_number(const char* input);
Token get_next_token(const char* input);
TokenArray* get_next_object(const char* input);
bool objsyntacticchecker(TokenArray* obj);

void free_token(Token* token) {
	token->type = 0;
	if (token->value) {
		free(token->value);
	}
}

TokenArray* initTokenArray(size_t initial_capacity) {
    TokenArray* arr = malloc(sizeof(*arr));
    arr->data = (Token *)malloc(initial_capacity * sizeof(Token));
    arr->size = 0;
	arr->capacity = initial_capacity;
    return arr;
}

void addToken(TokenArray *arr, Token new_token) {
    if (arr->size == arr->capacity) {
        arr->capacity *= 2; // Double the capacity
        arr->data = (Token *)realloc(arr->data, arr->capacity * sizeof(Token));
    }
    arr->data[arr->size++] = new_token;
}

Token* getToken(TokenArray *arr, size_t index) {
    if (index >= arr->size) {
        fprintf(stderr, "Error: index out of bounds\n");
        exit(EXIT_FAILURE);
    }
    return &(arr->data[index]);
}

void removeToken(TokenArray *arr, size_t index) {
    if (index >= arr->size) {
        fprintf(stderr, "Error: index out of bounds\n");
    }
    for (size_t i = index; i < arr->size - 1; i++) {
        arr->data[i] = arr->data[i + 1];
    }
    arr->size--;
}

void freeTokenArray(TokenArray *arr) {
    free(arr->data);
    arr->size = 0;
    arr->capacity = 0;
    free(arr);
}

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

TokenArray* get_next_object(const char* input) {
	TokenArray* obj = initTokenArray(OBJ_INIT_CAP);
	Token token;
	while ((token = get_next_token(input)).type != TOKEN_CURL_CLOSE) {
		if (token.type == TOKEN_EOF) {
			fprintf(stderr, "Error: '}' missing\n");
			exit(-1);
		}
		if (token.type != TOKEN_NL) {
			addToken(obj, token);
		}
	}
	return obj;
}

bool objsyntacticchecker(TokenArray* obj) {
	for (size_t i=0; i<obj->size; i++) {
		if (getToken(obj, i)->type == TOKEN_CURL_OPEN) {
			assert(getToken(obj, i+1)->type == TOKEN_STRING);
		} else if (getToken(obj, i)->type == TOKEN_STRING) {
			assert(getToken(obj, i+1)->type == TOKEN_COLON);
        }
	}
	return 0;
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
