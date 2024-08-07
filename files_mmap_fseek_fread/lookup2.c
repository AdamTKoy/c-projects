/**
 * mad_mad_access_patterns
 * CS 341 - Spring 2024
 */
#include "tree.h"
#include "utils.h"

// includes from mmap man page example:
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

/*
  Look up a few nodes in the tree and print the info they contain.
  This version uses mmap to access the data.

  ./lookup2 <data_file> <word> [<word> ...]
*/

// global variable for result from mmap
static void *addr;

// recursive function to search through branches of tree
// (using pointer arithmetic w/ mmap)
void searchTree(size_t offset, const char *word)
{

  BinaryTreeNode *nody = (BinaryTreeNode *)(addr + offset);

  int compare_value = strcmp(word, nody->word);

  if (compare_value == 0)
  {
    printFound(word, nody->count, nody->price);
  }

  else if (compare_value < 0)
  { // word is < this node's word: check left child

    // no left child exists, end
    if (nody->left_child == 0)
    {
      printNotFound(word);
    }

    // or recurse on existing left child
    else
    {
      searchTree(nody->left_child, word);
    }
  }

  else
  { // word is > this node's word: check right child

    // no right child exists, end
    if (nody->right_child == 0)
    {
      printNotFound(word);
    }

    // or recurse on existing right child
    else
    {
      searchTree(nody->right_child, word);
    }
  }
}

int main(int argc, char **argv)
{

  if (argc < 3)
  {
    printArgumentUsage();
    exit(1);
  }

  char *filename = argv[1];
  FILE *in_file = fopen(filename, "r");
  if (!in_file)
  {
    openFail(argv[1]);
    exit(2);
  }

  // file size reference:
  // https://www.geeksforgeeks.org/c-program-find-size-file/
  fseek(in_file, 0L, SEEK_END);
  long int sz = ftell(in_file);
  fseek(in_file, 0L, SEEK_SET); // reset

  // create mapping
  addr = mmap(0, sz, PROT_READ, MAP_SHARED, fileno(in_file), 0);

  fclose(in_file); // no longer need file since we have mapping

  // check for header string
  if (strncmp(addr, BINTREE_HEADER_STRING, BINTREE_ROOT_NODE_OFFSET) != 0)
  {
    formatFail(filename);
    exit(2);
  }

  for (int i = 0; i < (argc - 2); i++)
  {
    searchTree(BINTREE_ROOT_NODE_OFFSET, argv[i + 2]);
  }

  return 0;
}
