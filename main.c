#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>

#define BUF_SIZE 256
#define OBJ_INIT_CAP 24

typedef enum {
    // delimiters
	TOKEN_CURL_OPEN,
	TOKEN_CURL_CLOSE,
	TOKEN_BRACKET_OPEN,
	TOKEN_BRACKET_CLOSE,
	TOKEN_COMMA,
	TOKEN_COLON,
    // primite values
	TOKEN_STRING,
	TOKEN_NUMBER,
	TOKEN_TRUE,
	TOKEN_FALSE,
	TOKEN_NULL,
	TOKEN_NL,
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

typedef enum {
    NODE_OBJECT,
    NODE_ARRAY,
    NODE_STRING,
    NODE_NUMBER,
    NODE_BOOL,
    NODE_NULL
} JsonNodeType;

typedef struct JsonNode {
    JsonNodeType type;
    char* key;              // For object properties
    union {
        struct JsonNode** children; // For objects and arrays
        char* string_value;
        double number_value;
        bool bool_value;
    } value;
    size_t child_count;
} JsonNode;

void die(const char*fmt, ...);
Token* init_token(TokenType type, const char* value);
void free_token(Token* token);
TokenArray* initTokenArray(size_t initial_capacity);
void addToken(TokenArray* arr, const Token token);
Token* getToken(const TokenArray* arr, size_t index);
void removeToken(TokenArray* arr, size_t index);
void freeTokenArray(TokenArray* arr);
JsonNode* initJsonNode(JsonNodeType type, const char* key);
void free_json_node(JsonNode* node);
// lexer
Token* next_string(const char* json, size_t index);
Token* next_number(const char* json, size_t index);
Token* next_token(const char* json, size_t index);
// parser
JsonNode* parse_object();
JsonNode* parse_array();
JsonNode* parse_value();

void die(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

Token* init_token(TokenType type, const char* value) {
    Token* token = malloc(sizeof(*token));
    token->type = type;
    token->value = strdup(value);
    return token;
}

void free_token(Token* token) {
	token->type = 0;
	if (token->value) {
		free(token->value);
	}
    free(token);
}

TokenArray* initTokenArray(size_t initial_capacity) {
    TokenArray* arr = malloc(sizeof(*arr));
    arr->data = (Token *)malloc(initial_capacity * sizeof(Token));
    arr->size = 0;
	arr->capacity = initial_capacity;
    return arr;
}

void addToken(TokenArray *arr, const Token token) {
    if (arr->size == arr->capacity) {
        arr->capacity *= 2; // Double the capacity
        arr->data = (Token *)realloc(arr->data, arr->capacity * sizeof(*arr));
    }
    arr->data[arr->size++] = token;
}

Token* getToken(const TokenArray *arr, size_t index) {
    if (index >= arr->size) {
        die("Error: index out of bounds\n");
    }
    return &(arr->data[index]);
}

void removeToken(TokenArray *arr, size_t index) {
    if (index >= arr->size) {
        die("Error: index out of bounds\n");
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

JsonNode* initJsonNode(JsonNodeType type, const char* key) {
	JsonNode* node = malloc(sizeof(*node));
	node->type = type;
	node->key = strdup(key);
	node->child_count = 0;
	switch (type) {
		case NODE_OBJECT:
		case NODE_ARRAY:
			node->value.children = NULL;
			break;
		case NODE_STRING:
			node->value.string_value = NULL;
			break;
		case NODE_NUMBER:
			node->value.number_value = 0;
			break;
		case NODE_BOOL:
			node->value.bool_value = false;
			break;
		case NODE_NULL:
			break;
	}

	return node;
}

void free_json_node(JsonNode* node) {
	if (!node) return;
	if (node->type == NODE_OBJECT || node->type == NODE_ARRAY) {
		for (size_t i = 0; i < node->child_count; i++) {
			free_json_node(node->value.children[i]);
		}
		free(node->value.children);
	} else if (node->type == NODE_STRING) {
		free(node->value.string_value);
	}
	free(node->key);
	free(node);
}

Token* next_string(const char* json, size_t index) {
	char* string_value = strdup("");
	while (json[index++] != '\"') {
		// append on the fly const char* from char to string_value
		strcat(string_value, (char[2]){json[index-1], '\0'}); 
	}

	return init_token(TOKEN_STRING, string_value);
}

Token* next_number(const char* json, size_t index) {
	int is_negative = (json[index]=='-') ? 1 : 0;
	char* num_value = strdup("");

	index += is_negative;
	while (isdigit(json[index++])) {
		strcat(num_value, (char[2]){json[index-1], '\0'});
	}

	return init_token(TOKEN_NUMBER, num_value);
}

Token* next_token(const char* json, size_t index) {
	char buffer[5];
	int c;

	if (index >= strlen(json)) {
		return init_token(TOKEN_EOF, NULL);
	}
	// skip spaces and tabs
	while (json[index] == ' ' || json[index] == '\t') {
		index++;
	}
	switch (c = json[index]) {
	case '\"':
		index++;
		return next_string(json, index);
	case '{':
		index++;
		return init_token(TOKEN_CURL_OPEN, strdup("{"));
	case '}':
		index++;
		return init_token(TOKEN_CURL_CLOSE, strdup("}"));
	case '[':
		index++;
		return init_token(TOKEN_BRACKET_OPEN, strdup("["));
	case ']':
		index++;
		return init_token(TOKEN_BRACKET_CLOSE, strdup("]"));
	case ',':
		index++;
		return init_token(TOKEN_COMMA, strdup(","));
	case ':':
		index++;
		return init_token(TOKEN_COLON, strdup(":"));
	case '\0':
		index++;
		return init_token(TOKEN_EOF, NULL);
	default:
		strncpy(buffer, json+index, 5);
		if (!strncmp(buffer, "true", 4)) {
			index += 4;
			return init_token(TOKEN_TRUE, strdup("true"));
		} else if (!strncmp(buffer, "false", 5)) {
			index += 5;
			return init_token(TOKEN_FALSE, strdup("false"));
		} else if (!strncmp(buffer, "null", 4)) {
			index += 4;
			return init_token(TOKEN_NULL, strdup("null"));
		} else if (isdigit(c) || c == '-') {
			return next_number(json, index);
		} else if (c == '\n') {
			return init_token(TOKEN_NL, NULL);
		} else {
            return NULL;
		}
	}
}

int main(int argc, char** argv) {
	FILE* fp;
	
	if (argc == 1) {
		die("Error: which file?\n");
		exit(-1);
	} else {
		fp = fopen(argv[1], "r");
		if (fp == NULL) {
			die("Error: opening file\n");
			exit(-1);
		}
	}
	char* json = malloc(BUF_SIZE);
	Token* token = NULL;
	size_t lexer_index = 0;

	while ((fgets(json, BUF_SIZE, fp)) != NULL) {
		lexer_index = 0;
		if (json[strlen(json) - 1] != '\n' && !feof(fp)) {
			die("Error: Line too long for buffer.\n");
		}
		while (((token = next_token(json, lexer_index))->type != TOKEN_NL) && token->type != TOKEN_EOF) {
			if (token == NULL) {
				die("Error: %s\n", token->value);
				free_token(token);
				break;
			} else if (token->type == TOKEN_EOF) {
				break;
			}
			printf("Token type: %d", token->type);
			if (token->value) {
				printf(" value: %s\n", token->value);
			} else {
				printf("\n");
			}
		}
		free_token(token);
	}

	free(json);
	fclose(fp);
	return 0;
}
