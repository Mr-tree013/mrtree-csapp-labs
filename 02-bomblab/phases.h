#ifndef PHASES_H
#define PHASES_H

/* BST 节点 — 必须在 fun7 声明之前定义 */
struct bst_node {
    int value;
    int _pad;
    struct bst_node *left;
    struct bst_node *right;
};

void phase_1(const char *input);
void phase_2(const char *input);
void phase_3(const char *input);
void phase_4(const char *input);
void phase_5(const char *input);
void phase_6(const char *input);
void secret_phase(void);

int func4(int target, int lo, int hi);
int fun7(struct bst_node *node, int target);

#endif
