#include "outputbuilder.h"
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <stddef.h>

// tokenizer
typedef enum
{
    TOK_END = 0,
    TOK_VAR,
    TOK_NOT,
    TOK_AND,
    TOK_OR,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_XOR
} TokenType;

typedef struct
{
    TokenType type;
    char var; // 'A', 'B', or 'C' for TOK_VAR, undefined otherwise
} Token;

#define MAX_TOKENS 64

// error codes
#define ERR_OK 0
#define ERR_SYNTAX 1
#define ERR_UNKNOWN_CHAR 2
#define ERR_TOKEN_OVERFLOW 3
#define ERR_NODE_POOL 4

static int tokenize(const char *expr, Token tokens[], int max_tokens, int *out_count)
{
    int count = 0;

    for (int i = 0; expr[i] != '\0'; ++i)
    {
        char c = expr[i];

        // whitespace skip
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
            continue;
        }

        if (count >= max_tokens - 1)
        {
            return ERR_TOKEN_OVERFLOW;
        }

        if (c == '!')
        {
            tokens[count].type = TOK_NOT;
            tokens[count].var = 0;
            count++;
        }
        else if (c == '&')
        {
            tokens[count].type = TOK_AND;
            tokens[count].var = 0;
            count++;
        }
        else if (c == '|')
        {
            tokens[count].type = TOK_OR;
            tokens[count].var = 0;
            count++;
        }
        else if (c == '^')
        {
            tokens[count].type = TOK_XOR;
            tokens[count].var = 0;
            count++;
        }
        else if (c == '(')
        {
            tokens[count].type = TOK_LPAREN;
            tokens[count].var = 0;
            count++;
        }
        else if (c == ')')
        {
            tokens[count].type = TOK_RPAREN;
            tokens[count].var = 0;
            count++;
        }
        else
        {
            // Variables: allow A/B/C (case-insensitive)
            char u = (char)toupper((unsigned char)c);
            if (u == 'A' || u == 'B' || u == 'C')
            {
                tokens[count].type = TOK_VAR;
                tokens[count].var = u;
                count++;
            }
            else
            {
                return ERR_UNKNOWN_CHAR;
            }
        }
    }

    // EOT input
    tokens[count].type = TOK_END;
    tokens[count].var = 0;
    count++;

    *out_count = count;
    return ERR_OK;
}

// AST Node Pool

typedef enum
{
    NODE_VAR = 0,
    NODE_NOT,
    NODE_AND,
    NODE_OR,
    NODE_XOR
} NodeType;

typedef struct Node
{
    NodeType type;
    uint8_t var_index; // 0 = A, 1 = B, 2 = C (for NODE_VAR)
    struct Node *left; // For NOT, use left as the single child
    struct Node *right;
} Node;

#define MAX_NODES 64

static Node node_pool[MAX_NODES];
static int node_count = 0;

static void reset_node_pool(void)
{
    node_count = 0;
}

static Node *alloc_node(void)
{
    if (node_count >= MAX_NODES)
    {
        return NULL;
    }
    Node *n = &node_pool[node_count++];
    n->type = NODE_VAR;
    n->var_index = 0;
    n->left = NULL;
    n->right = NULL;
    return n;
}

// recurisve descent parser
typedef struct
{
    Token *tokens;
    int pos;
    int count;
    int error; // any nonzero indicates error code
} Parser;

static Token *current_token(Parser *p)
{
    if (p->pos < p->count)
    {
        return &p->tokens[p->pos];
    }
    // if out of range, dummy end token
    static Token dummy = {TOK_END, 0};
    return &dummy;
}

static bool accept(Parser *p, TokenType type)
{
    if (current_token(p)->type == type)
    {
        p->pos++;
        return true;
    }
    return false;
}

static Node *parse_expr(Parser *p); // forward declaration
static Node *parse_term(Parser *p);
static Node *parse_factor(Parser *p);
static Node *parse_xor(Parser *p);

// factor := '!' factor | VAR | '(' expr ')'
static Node *parse_factor(Parser *p)
{
    Token *t = current_token(p);

    /* '!' factor */
    if (t->type == TOK_NOT)
    {
        p->pos++; // consume '!'
        Node *child = parse_factor(p);
        if (!child)
        {
            return NULL;
        }
        Node *n = alloc_node();
        if (!n)
        {
            p->error = ERR_NODE_POOL;
            return NULL;
        }
        n->type = NODE_NOT;
        n->left = child; // use left as single child
        n->right = NULL;

        return n;
    }
    // VAR
    else if (t->type == TOK_VAR)
    {
        Node *n = alloc_node();
        if (!n)
        {
            p->error = ERR_NODE_POOL;
            return NULL;
        }
        char v = t->var;
        uint8_t idx = 0;
        if (v == 'A')
        {
            idx = 0;
        }
        else if (v == 'B')
        {
            idx = 1;
        }
        else if (v == 'C')
        {
            idx = 2;
        }
        else
        {
            p->error = ERR_SYNTAX;
            return NULL;
        }
        n->type = NODE_VAR;
        n->var_index = idx;
        n->left = NULL;
        n->right = NULL;
        p->pos++; // consume VAR

        return n;
    }
    /* '(' expr ')' */
    else if (t->type == TOK_LPAREN)
    {
        p->pos++; // consume '('
        Node *inside = parse_expr(p);
        if (!inside)
        {
            return NULL;
        }
        if (!accept(p, TOK_RPAREN))
        {
            p->error = ERR_SYNTAX;
            return NULL;
        }
        return inside;
    }
    // Unexpected token
    else
    {
        p->error = ERR_SYNTAX;
        return NULL;
    }
}

/* term := factor { '&' factor } */
static Node *parse_term(Parser *p)
{
    Node *left = parse_factor(p);
    if (!left)
        return NULL;

    while (current_token(p)->type == TOK_AND)
    {
        p->pos++; /* consume '&' */
        Node *right = parse_factor(p);
        if (!right)
            return NULL;

        Node *n = alloc_node();
        if (!n)
        {
            p->error = ERR_NODE_POOL;
            return NULL;
        }
        n->type = NODE_AND;
        n->left = left;
        n->right = right;
        left = n; /* new subtree root */
    }
    return left;
}

/* expr := xor { '|' xor } */
static Node *parse_expr(Parser *p)
{
    Node *left = parse_xor(p);
    if (!left)
        return NULL;

    while (current_token(p)->type == TOK_OR)
    {
        p->pos++; /* consume '|' */
        Node *right = parse_xor(p);
        if (!right)
            return NULL;

        Node *n = alloc_node();
        if (!n)
        {
            p->error = ERR_NODE_POOL;
            return NULL;
        }
        n->type = NODE_OR;
        n->left = left;
        n->right = right;
        left = n;
    }
    return left;
}

/* xor := term { '^' term } */
static Node *parse_xor(Parser *p)
{
    Node *left = parse_term(p);
    if (!left)
        return NULL;

    while (current_token(p)->type == TOK_XOR)
    {
        p->pos++; /* consume '^' */
        Node *right = parse_term(p);
        if (!right)
            return NULL;

        Node *n = alloc_node();
        if (!n)
        {
            p->error = ERR_NODE_POOL;
            return NULL;
        }
        n->type = NODE_XOR;
        n->left = left;
        n->right = right;
        left = n; /* new subtree root */
    }
    return left;
}

// AST Evaluator

int eval_ast(const Node *root, int A, int B, int C)
{
    if (!root)
    {
        return 0;
    }

    switch (root->type)
    {
    case NODE_VAR:
    {
        int vars[3] = {A ? 1 : 0, B ? 1 : 0, C ? 1 : 0};
        uint8_t idx = root->var_index;
        if (idx > 2)
        {
            return 0;
        }
        return vars[idx];
    }
    case NODE_NOT:
    {
        int v = eval_ast(root->left, A, B, C);
        return v ? 0 : 1;
    }
    case NODE_AND:
    {
        int lv = eval_ast(root->left, A, B, C);
        if (!lv)
        {
            return 0; // short circuit
        }
        int rv = eval_ast(root->right, A, B, C);

        return (lv && rv) ? 1 : 0;
    }
    case NODE_OR:
    {
        int lv = eval_ast(root->left, A, B, C);
        if (lv)
        {
            return 1; // short-circuit
        }
        int rv = eval_ast(root->right, A, B, C);
        return (lv || rv) ? 1 : 0;
    }
    case NODE_XOR:
    {
        int lv = eval_ast(root->left, A, B, C);
        int rv = eval_ast(root->right, A, B, C);
        // for 0/1 values, XOR is just "not equal"
        return (lv != rv) ? 1 : 0;
    }
    default:
        return 0;
    }
}

// public API: build_truth_table

int build_truth_table(const char *expr, uint8_t outputs[8])
{
    Token tokens[MAX_TOKENS];
    int token_count = 0;

    int tok_err = tokenize(expr, tokens, MAX_TOKENS, &token_count);
    if (tok_err != ERR_OK)
    {
        return tok_err; // invalid char or token overflow
    }

    reset_node_pool();

    Parser p;
    p.tokens = tokens;
    p.pos = 0;
    p.count = token_count;
    p.error = ERR_OK;

    Node *root = parse_expr(&p);
    if (!root || p.error != ERR_OK)
    {
        return p.error ? p.error : ERR_SYNTAX;
    }

    // after parsing, we must be exactly at EOI
    if (current_token(&p)->type != TOK_END)
    {
        return ERR_SYNTAX;
    }

    // evalfor all 8 combinations into a temp buffer first
    uint8_t tmp[8];
    for (int row = 0; row < 8; ++row)
    {
        int A = (row >> 2) & 1; // MSB
        int B = (row >> 1) & 1;
        int C = (row >> 0) & 1; // LSB

        int v = eval_ast(root, A, B, C);
        tmp[row] = (uint8_t)(v ? 1 : 0);
    }

    // only copy to outputs on success
    for (int i = 0; i < 8; ++i)
    {
        outputs[i] = tmp[i];
    }

    return ERR_OK; // success
}
