/**
 * mad_mad_access_patterns
 * CS 341 - Spring 2024
 */
#include "tree.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
  Look up a few nodes in the tree and print the info they contain.
  This version uses fseek() and fread() to access the data.

  ./lookup1 <data_file> <word> [<word> ...]
*/

static FILE *in_file;
static char *filename;

void searchTree(size_t offset, const char *word);

int main(int argc, char **argv)
{
  // check for at least 2 arguments after ./lookup(1/2)
  if (argc < 3)
  {
    printArgumentUsage();
    exit(1);
  }

  // try to open file for reading
  filename = argv[1];
  in_file = fopen(filename, "r");
  if (!in_file)
  {
    openFail(argv[1]);
    exit(2);
  }

  // check if first 4 bytes are "BTRE"
  char first_four_buf[BINTREE_ROOT_NODE_OFFSET + 1];
  size_t first_four = fread(first_four_buf, BINTREE_ROOT_NODE_OFFSET, 1, in_file);

  // check for format fail
  if (first_four != 1)
  {
    formatFail(filename);
    exit(2);
  }
  if (strncmp(first_four_buf, BINTREE_HEADER_STRING, BINTREE_ROOT_NODE_OFFSET) != 0)
  {
    formatFail(filename);
    exit(2);
  }

  // (argc - 2) should be number of search words
  for (int i = 0; i < (argc - 2); i++)
  {
    searchTree(BINTREE_ROOT_NODE_OFFSET, argv[i + 2]);
  }

  fclose(in_file);

  return 0;
}

// function to look through valid paths of tree for each search word
void searchTree(size_t offset, const char *word)
{

  int status = fseek(in_file, offset, SEEK_SET);
  if (status != 0)
  {
    formatFail(filename);
    exit(2);
  }

  BinaryTreeNode nody;
  fread(&nody, sizeof(BinaryTreeNode), 1, in_file);

  // node's 'word' is offset from node start by 16 bytes
  status = fseek(in_file, offset + 16, SEEK_SET);
  if (status != 0)
  {
    formatFail(filename);
    exit(2);
  }

  // get 'word' at nody
  char nody_word[256];

  int done = 0;
  int idx = 0;
  int c;

  while (!done)
  {
    c = fgetc(in_file);

    if (c != '\0')
    {
      nody_word[idx] = c;
      idx++;
    }
    else
    {
      nody_word[idx] = '\0';
      done = 1;
      break;
    }
  }

  char *nw = (char *)&nody_word;
  int compare_value = strcmp(word, nw);

  if (compare_value == 0)
  {
    printFound(word, nody.count, nody.price);
  }

  else if (compare_value < 0)
  { // word is < this node's word: check left child

    // no left child exists, end
    if (nody.left_child == 0)
    {
      printNotFound(word);
    }

    // or recurse on existing left child
    else
    {
      searchTree(nody.left_child, word);
    }
  }

  else
  { // word is > this node's word: check right child

    // no right child exists, end
    if (nody.right_child == 0)
    {
      printNotFound(word);
    }

    // or recurse on existing right child
    else
    {
      searchTree(nody.right_child, word);
    }
  }
}