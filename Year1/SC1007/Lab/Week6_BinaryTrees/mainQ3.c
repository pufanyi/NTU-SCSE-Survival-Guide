#include <stdio.h>
#include <stdlib.h>

typedef struct _btnode
{
    int item;
    struct _btnode *left;
    struct _btnode *right;
} BTNode;

BTNode *insertBTNode(BTNode *cur, int item);
void printBTNode(BTNode *root, int space, int left);
void deleteTree(BTNode **root);

void mirrorTree(BTNode *node);

int main()
{
    BTNode *root = NULL;
    int item;

    printf("Enter a list of numbers for a Binary Tree, terminated by any non-digit character: \n");
    while (scanf("%d", &item))
        root = insertBTNode(root, item);
    scanf("%*s");

    printf("Original Tree\n");
    printBTNode(root, 0, 0);
    printf("---------------------------------------------------------------\n");
    mirrorTree(root);
    printBTNode(root, 0, 0);
    printf("Mirrored Tree\n");

    deleteTree(&root);
    root = NULL;

    return 0;
}

BTNode *insertBTNode(BTNode *cur, int item)
{
    if (cur == NULL)
    {
        BTNode *node = (BTNode *)malloc(sizeof(BTNode));
        node->item = item;
        node->left = node->right = NULL;
        return node;
    }
    if (rand() % 2)
        cur->left = insertBTNode(cur->left, item);
    else
        cur->right = insertBTNode(cur->right, item);

    return cur;
}
void printBTNode(BTNode *root, int space, int left)
{
    if (root != NULL)
    {

        int i;
        for (i = 0; i < space - 1; i++)
            printf("|\t");

        if (i < space)
        {
            if (left == 1)
                printf("|---");
            else
                printf("|___");
        }

        printf("%d\n", root->item);

        space++;
        printBTNode(root->left, space, 1);
        printBTNode(root->right, space, 0);
    }
}

void deleteTree(BTNode **root)
{
    if (*root != NULL)
    {
        deleteTree(&((*root)->left));
        deleteTree(&((*root)->right));
        free(*root);
        *root = NULL;
    }
}

void mirrorTree(BTNode *node)
{
    // Write your code here
    if(node == NULL) return;
    BTNode *temp = NULL;
    temp = node->left;
    node->left = node->right;
    node->right = temp;

    mirrorTree(node->left);
    mirrorTree(node->right);
}
