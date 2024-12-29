#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>

#define BUF_INIT_SIZE 256
#define ARR_INIT_SIZE 24

typedef enum {
	TOKEN_CURL_OPEN,
	TOKEN_BRACKET_OPEN,
	TOKEN_STRING,
	TOKEN_NUMBER,
	TOKEN_TRUE,
	TOKEN_FALSE,
	TOKEN_NULL,
	TOKEN_CURL_CLOSE,
	TOKEN_BRACKET_CLOSE,
	TOKEN_COMMA,
	TOKEN_COLON,
	TOKEN_EOF
} TokenType;

typedef struct {
	TokenType type;
	char* value;
} Token;

typedef struct {
	Token** tokens;
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
TokenArray* init_token_array(size_t initial_capacity);
void cp_token(Token* dst, const Token* src);
void add_token(TokenArray* arr, const Token* token);
Token* get_token(const TokenArray* arr, size_t index);
void remove_token(TokenArray* arr, size_t index);
void free_token_array(TokenArray* arr);
JsonNode* init_json_node(JsonNodeType type, const char* key);
void free_json_node(JsonNode* node);
// lexer
Token* next_string(const char* json, size_t* index);
Token* next_number(const char* json, size_t* index);
Token* next_token(const char* json, size_t* index);
// syntax_checker
bool token_is_value(Token* token);
void syntax_checker(TokenArray* arr);
// parser
JsonNode* parse(TokenArray* arr, size_t start, JsonNodeType);
TokenArray* tokenize(const char* json);
void dump_file(FILE* fp, char* buffer, size_t size);

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
    token->value = value ? strdup(value) : NULL;
    return token;
}

void free_token(Token* token) {
	token->type = 0;
	if (token->value) {
		free(token->value);
	}
    free(token);
}

TokenArray* init_token_array(size_t initial_capacity) {
    TokenArray* arr = malloc(sizeof(*arr));
    arr->tokens = (Token **)malloc(initial_capacity * sizeof(Token*));
    arr->size = 0;
	arr->capacity = initial_capacity;
    return arr;
}

void cp_token(Token* dst, const Token* src) {
	dst->type = src->type;
	dst->value = src->value ? strdup(src->value) : NULL;
}

void add_token(TokenArray *arr, const Token* token) {
    if (arr->size == arr->capacity) {
        arr->capacity *= 2; // Double the capacity
        arr->tokens = (Token **)realloc(arr->tokens, arr->capacity * sizeof(Token*));
    }
	arr->tokens[++(arr->size)] = init_token(TOKEN_NULL, NULL);
    cp_token(arr->tokens[arr->size], token);
}

void remove_token(TokenArray *arr, size_t index) {
    if (index >= arr->size) {
        die("Error: index out of bounds\n");
    }
    for (size_t i = index; i < arr->size - 1; i++) {
        arr->tokens[i] = arr->tokens[i + 1];
    }
    arr->size--;
}

void free_token_array(TokenArray *arr) {
    free(arr->tokens);
    arr->size = 0;
    arr->capacity = 0;
    free(arr);
}

JsonNode* init_json_node(JsonNodeType type, const char* key) {
	JsonNode* node = malloc(sizeof(*node));
	node->type = type;
	node->key =  key ? strdup(key) : NULL ;
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

Token* next_string(const char* json, size_t* index) {
	char* string_value = strdup("");
	while (json[*(index)++] != '\"') {
		// append on the fly const char* from char to string_value
		strcat(string_value, (char[2]){json[*index-1], '\0'}); 
	}

	return init_token(TOKEN_STRING, string_value);
}

Token* next_number(const char* json, size_t* index) {
	int is_negative = (json[*index]=='-') ? 1 : 0;
	char* num_value = strdup("");

	index += is_negative;
	while (isdigit(json[(*index)++])) {
		strcat(num_value, (char[2]){json[*index-1], '\0'});
	}

	return init_token(TOKEN_NUMBER, num_value);
}

Token* next_token(const char* json, size_t* index) {
	char buffer[5];
	int c;
	TokenArray* arr = init_token_array(ARR_INIT_SIZE);
	Token* token = init_token(TOKEN_NULL, NULL);

	if (*index >= strlen(json)) {
		return init_token(TOKEN_EOF, NULL);
	}
	// skip spaces and tabs
	while (json[*index] == ' ' || json[*index] == '\t') {
		*(index)++;
	}
	switch (c = json[*index]) {
	case '\"':
		(*index)++;
		return next_string(json, index);
	case '{':
		(*index)++;
		while ((token = next_token(json, index))->type != TOKEN_CURL_CLOSE) {
			add_token(arr, token);
		}
		return init_token(TOKEN_CURL_OPEN, strdup("{"));
	case '}':
		(*index)++;
		return init_token(TOKEN_CURL_CLOSE, strdup("}"));
	case '[':
		(*index)++;
		while ((token = next_token(json, index))->type != TOKEN_BRACKET_CLOSE) {
			add_token(arr, token);
		}
		return init_token(TOKEN_BRACKET_OPEN, strdup("["));
	case ']':
		(*index)++;
		return init_token(TOKEN_BRACKET_CLOSE, strdup("]"));
	case ',':
		(*index)++;
		return init_token(TOKEN_COMMA, strdup(","));
	case ':':
		(*index)++;
		return init_token(TOKEN_COLON, strdup(":"));
	case '\0':
		(*index)++;
		return init_token(TOKEN_EOF, NULL);
	default:
		strncpy(buffer, json+(*index), 5);
		if (!strncmp(buffer, "true", 4)) {
			*index += 4;
			return init_token(TOKEN_TRUE, strdup("true"));
		} else if (!strncmp(buffer, "false", 5)) {
			*index += 5;
			return init_token(TOKEN_FALSE, strdup("false"));
		} else if (!strncmp(buffer, "null", 4)) {
			*index += 4;
			return init_token(TOKEN_NULL, strdup("null"));
		} else if (isdigit(c) || c == '-') {
			return next_number(json, index);
		} else {
			die("error reading token: %s", json+(*index));
			return NULL;
		}
	}
}

bool token_is_value(Token* token) {
	return (0 <= token->type && token->type <= 6);
}

void syntax_checker(TokenArray* arr) {
	Token* token = init_token(TOKEN_NULL, NULL);
	bool in_object, in_array = false;
	for (size_t i = 0; i < arr->size; i++) {
		token = arr->tokens[i];
		if (token->type == TOKEN_CURL_OPEN && (token+1)->type != TOKEN_STRING) {
			in_array = false;
			in_object = true;
			die("missing key after '{'");
		} else if (token->type == TOKEN_BRACKET_OPEN && !token_is_value(token+1)) {
			in_array = true;
			in_object = false;
			die("missing value at entry of array");
		} else if (!in_array && !in_object) {
			die("error");
		}
		if (in_object) {
			if (token->type == TOKEN_STRING && (token+1)->type != TOKEN_COLON) {
				die("missing ':' after key value");
			} else if (token->type == TOKEN_COLON && !token_is_value(token)) {
				die("missing value after ':'");
			} else if (token_is_value(token) && ((token+1)->type != TOKEN_COMMA || (token+1)->type != TOKEN_CURL_CLOSE)) {
				die("unexpected end of object");
			} else if (token->type == TOKEN_COMMA && (token+1)->type != TOKEN_STRING) {
				die("missing key after ','");
			}
		} else if (in_array) {
			if (token_is_value(token) && (token+1)->type != TOKEN_COMMA) {
				die("missing value after ','");
			}
		} 
	}
}

JsonNode* parse(TokenArray* arr, size_t start, JsonNodeType type) {
	JsonNode* node = init_json_node(type, NULL);
	Token* token;

	syntax_checker(arr);

	for (size_t i = start; i < arr->size; i++) {
		token = arr->tokens[i];
		switch (token->type) {
			case TOKEN_STRING:
				if (node->key == NULL) {
					node->key = strdup(token->value);
				} else {
					node->value.string_value = strdup(token->value);
				}
				break;
			case TOKEN_NUMBER:
				node->value.number_value = atoi(token->value);
				break;
			case TOKEN_TRUE:
				node->value.bool_value = true;
				break;
			case TOKEN_FALSE:
				node->value.bool_value = false;
				break;
			case TOKEN_NULL:
				node->value.string_value = NULL;
				break;
			// child array
			case TOKEN_BRACKET_OPEN:
				node->value.children[node->child_count++] = parse(arr, i, NODE_ARRAY);
				node->child_count++;
				break;
			// child object
			case TOKEN_CURL_OPEN:
				node->value.children[node->child_count++] = parse(arr, i, NODE_OBJECT);
				break;
			case TOKEN_CURL_CLOSE:
			case TOKEN_BRACKET_CLOSE:
				return node;
			case TOKEN_COLON:
			case TOKEN_COMMA:
			case TOKEN_EOF:
				break;
			default:
				return NULL;
		}
	}
	return node;
}

TokenArray* tokenize(const char* json) {
	size_t lexer_index = 0;
	Token* token = init_token(TOKEN_NULL, NULL);
	TokenArray* arr = init_token_array(ARR_INIT_SIZE);

	while ((token = next_token(json, &lexer_index))->type != TOKEN_EOF) {
		add_token(arr, token);
		free(token);
	}

	return arr;
}

void dump_file(FILE* fp, char* buffer, size_t size) {
	char c;

	while ((c = getc(fp)) != EOF) {
		if (strlen(buffer) >= size) {
			buffer = realloc(buffer, 2 * size);
		}
		if (c != '\n' && c != ' ' && c != '\t') {
			strncat(buffer, (char[2]){c, '\0'}, size);
		}
	}
}

int main(int argc, char** argv) {
	FILE* fp;
	char* buffer = malloc(BUF_INIT_SIZE);
	
	if (argc == 1)
		die("Error: which file?\n");

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		die("Error: opening file %s\n", argv[1]);
	}

	dump_file(fp, buffer, BUF_INIT_SIZE);
	TokenArray* arr = tokenize(buffer);

	fclose(fp);
	return 0;
}
