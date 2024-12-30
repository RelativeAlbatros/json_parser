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
	bool is_value;
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
void syntax_checker(TokenArray* arr);
// parser
JsonNode* parse(TokenArray* arr, size_t start);
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
	token->is_value = false;
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
	if (token) {
		arr->tokens[(++(arr->size)) - 1] = init_token(TOKEN_NULL, NULL);
		cp_token(arr->tokens[arr->size - 1], token);
	} else {
		die("returned NULL token");
	}
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
	char* string_value = malloc(BUF_INIT_SIZE);
	while (json[*index] != '\"') {
		// append on the fly const char* from char to string_value
		strcat(string_value, (char[2]){json[*index], '\0'}); 
		(*index)++;
	}

	(*index)++; // skip closing "
	return init_token(TOKEN_STRING, string_value);
}

Token* next_number(const char* json, size_t* index) {
	int is_negative = (json[*index]=='-') ? 1 : 0;
	char* num_value = malloc(BUF_INIT_SIZE);

	index += is_negative;
	while (isdigit(json[*index])) {
		strcat(num_value, (char[2]){json[*index-1], '\0'});
		(*index)++;
	}

	return init_token(TOKEN_NUMBER, num_value);
}

Token* next_token(const char* json, size_t* index) {
	char buffer[5];
	char c;

	if (*index > strlen(json)) {
		return init_token(TOKEN_EOF, NULL);
	}
	// skip spaces and tabs
	while (json[*index] == ' ' || json[*index] == '\t') {
		(*index)++;
	}
	switch (c = json[*index]) {
	case '\"':
		(*index)++;
		return next_string(json, index);
	case '{':
		(*index)++;
		return init_token(TOKEN_CURL_OPEN, strdup("{"));
	case '}':
		(*index)++;
		return init_token(TOKEN_CURL_CLOSE, strdup("}"));
	case '[':
		(*index)++;
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
			char* buff = malloc(BUF_INIT_SIZE);
			while(isalpha(json[*index])) {
				strcat(buff, (char[2]){json[(*index)++], '\0'});
			}
			die("error reading token: %s", buff);
			return NULL;
		}
	}
}

void syntax_checker(TokenArray* arr) {
	Token* token;
	Token* nxt_token;
	bool in_object = false, in_array = false;

	for (size_t i = 0; i < arr->size-1; i++) {
		token = arr->tokens[i];
		nxt_token = arr->tokens[i+1];

		if (token == NULL || nxt_token == NULL) {
			die("Syntax error: missing token");
		}

		if (token->type == TOKEN_CURL_OPEN) {
			in_array = false;
			in_object = true;
		} else if (token->type == TOKEN_BRACKET_OPEN) {
			in_array = true;
			in_object = false;
		} else if (token->type == TOKEN_BRACKET_CLOSE || token->type == TOKEN_CURL_CLOSE) {
			in_array = false;
			in_object = false;
		}

		if (token->type == TOKEN_CURL_OPEN) {
			if ((nxt_token->type != TOKEN_STRING && nxt_token->type != TOKEN_CURL_CLOSE)) {
				die("Syntax error: missing key after '{'");
			}
		} else if (token->type == TOKEN_BRACKET_OPEN) {
			if (token->is_value) {
				die("Syntax error: missing value at entry of array");
			}
		}
		if (in_object) {
			if (token->type == TOKEN_STRING && !token->is_value) {
				if (nxt_token->type != TOKEN_COLON) {
					die("Syntax error: missing ':' after key");
				} else if (0 <= arr->tokens[i+2]->type && arr->tokens[i+2]->type <= 6) {
					// check if primitive value or object or array
					// in which case it is a value
					arr->tokens[i+2]->is_value = true;
				}
			} else if (token->type == TOKEN_COLON) {
				if (!nxt_token->is_value) {
					die("Syntax error: missing value after ':'");
				}
			} else if (token->is_value && (token->type != TOKEN_CURL_OPEN && token->type != TOKEN_BRACKET_OPEN)) {
				if (nxt_token->type != TOKEN_COMMA && nxt_token->type != TOKEN_CURL_CLOSE) {
					die("Syntax error: unexpected end of object");
				}
			} else if (token->type == TOKEN_COMMA) {
				if (nxt_token->type != TOKEN_STRING) {
					die("Syntax error: missing key after ','");
				}
			}
		} else if (in_array) {
			if (token->is_value) {
				if (nxt_token->type != TOKEN_COMMA && nxt_token->type != TOKEN_BRACKET_CLOSE) {
					die("Syntax error: missing ',' after value");
				}
			}
		} 
		if (arr->tokens[arr->size-1]->type !=TOKEN_CURL_CLOSE &&
				arr->tokens[arr->size-1]->type !=TOKEN_BRACKET_CLOSE) {
			die("Syntax error: unclosed object or array");
		}
	}
}

JsonNode* parse(TokenArray* arr, size_t start) {
	JsonNodeType type;
	Token* token;

	syntax_checker(arr);

	switch (arr->tokens[start]->type) {
		case TOKEN_CURL_OPEN:
			type = NODE_OBJECT;
			start++;
			break;
		case TOKEN_BRACKET_OPEN:
			type = NODE_ARRAY;
			start++;
			break;
		default:
			die("expected object or array");
	}

	JsonNode* node = init_json_node(type, NULL);

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
				node->value.children[node->child_count++] = parse(arr, i);
				break;
			// child object
			case TOKEN_CURL_OPEN:
				node->value.children[node->child_count++] = parse(arr, i);
				break;
			case TOKEN_CURL_CLOSE:
			case TOKEN_BRACKET_CLOSE:
				return node;
			case TOKEN_COLON:
			case TOKEN_COMMA:
				break;
			case TOKEN_EOF:
				return NULL;
		}
	}
	return node;
}

TokenArray* tokenize(const char* json) {
	size_t lexer_index = 0;
	Token* token;
	TokenArray* arr = init_token_array(ARR_INIT_SIZE);

	while ((token = next_token(json, &lexer_index))->type != TOKEN_EOF) {
		add_token(arr, token);
	}
	if (arr->size == 0)
		die("empty json text");

	free(token);
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
	
	if (argc == 1) {
		die("Error: which file?\n");
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		die("Error: opening file %s\n", argv[1]);
	}

	char* buffer = malloc(BUF_INIT_SIZE);
	dump_file(fp, buffer, BUF_INIT_SIZE);
	TokenArray* arr = tokenize(buffer);
	free(buffer);
	JsonNode* parsed_json = parse(arr, 0);
	free_token_array(arr);
	free_json_node(parsed_json);

	fclose(fp);
	printf("all good!\n");
	return 0;
}
