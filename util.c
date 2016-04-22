#include "type.h"

void get_block(int dev_ref, int blk, char buf[BLKSIZE])
{
	lseek(dev_ref, (long)(blk*BLKSIZE), 0);
	read(dev_ref, buf, BLKSIZE);
}

int put_block(int dev_ref, int blk, char buf[BLKSIZE])
{
	lseek(dev_ref, (long)(blk*BLKSIZE), 0);
	write(dev_ref, buf, BLKSIZE);
}

int m_tokenize(char *path)
{
	printf("+Tokenize: \n");
	char *tok;
	tok = strtok(path, "/"); // gets first token from path
	extern int n;
	extern char names[64][64];
	extern char *name[64];
	n = 0;	
	while (tok != NULL) 
	{
		strncpy(names[n], tok, sizeof(names[n])); // copy token to names[i]
		name[n] = &names[n]; // set pointer in name to point to names[i]
		printf("\tDEBUG: tok = %s, n = %d, name[n] = %s, names[n] = %s\n", tok, n, name[n], names + n); //DEBUG
		tok = strtok(NULL, "/"); //get next token
		n++; //increment number of tokens
	}
	printf("-Tokenize: \n");
}

int m_search(MINODE *mip, char *name)
{
	printf("+search: \n");

	int i;	
	char buf[BLKSIZE];
	DIR *dp = (DIR *)buf;  // access buf[] as dir entries
	char *cp = buf;  // char pointer pointing at buf[]

	for(i = 0; i < 12; i++)
	{
		printf("\tDEBUG: searching i_block[%d] = %d for %s\n", i, mip->INODE.i_block[i], name);
		printf("\tDEBUG: ino\trec_len\tname_len\tname\n");
		printf("DEBUG: mip->dev = %d\n", mip->dev);
		get_block(mip->dev, mip->INODE.i_block[i], buf); // read a block into buf
		while(cp < buf + BLKSIZE)  // check the whole buffer
		{
			printf("\tDEBUG: %d \t%d \t%d \t\t%s \n", dp->inode, dp->rec_len, dp->name_len, dp->name);
			if(strncmp(dp->name, name, strlen(name)) == 0 && dp->name_len == strlen(name))  // check to see if names match
			{
				printf("\tDEBUG: name %s found at %d \n", name, dp->inode);
				printf("-search: \n");
				return dp->inode;
			}
			cp += dp->rec_len;  // increment char pointer by lenth of record
			dp = (DIR *)cp;		// 
		}
	}
	printf("\tDEBUG: name not found.\n");
	printf("-search: \n");
	return 0;
}

// Releases a Minode[]
int m_iput(MINODE *mip)
{
  char buf[BLKSIZE];
  extern char pathname[128];
  extern int inode_start;
  extern int n;
  int i;
  int block, inumber;
  int offset;
	int count = mip->refCount - 1;
	if (count > 0)
		return;
	if (mip->dirty == 0)
		return;
  if(mip->refCount == 0)
  {
			//mailmans
      block = ((mip->ino - 1) / 8) + inode_start;
      offset = (mip->ino - 1) % 8;
			printf("inode %d set\n", mip->ino);
			get_block(mip->dev, block, buf);
      INODE *ip = (INODE *)buf + offset;
      
			// put inode back on block
			memcpy(ip, &mip->INODE, sizeof(INODE)); // &mip->INODE(want pointer to INODE)
			put_block(mip->dev, block, buf);
	}
}

//takes an inode and a device, checks the minode array for that inode, 
//if it is found, it returns the MINODE it is in and increments the 
//refs by 1, if not, it uses the mailman algorithm to get the inode 
//sets up the new minode and returns that.

MINODE *m_iget(int dev_ref, int ino) // takes a device and ino and returns the corresponding MINODE
{
	printf("+iget: \n");
	int i = 0;
	int free = 0;
	int foundFree = 0;
	extern MINODE minode[NMINODE];  // array of MINODES 
	// search for the minode in the minode array
	while(i != NMINODE)
	{
		//printf("\tDEBUG: searching minode[%d]...\n", i);
		if(minode[i].refCount != 0)
		{
			if (minode[i].dev == dev_ref && minode[i].ino == ino)
			{
				minode[i].refCount++;
				//printf("\tDEBUG: minode found. refcount: %d\n", minode[i].refCount);
				printf("-iget: \n");
				return &minode[i];
			}
		}
		else if(foundFree == 0) // find first free inode for use if MINODE is not found
		{
			free = i;
			//printf("\tDEBUG: first free minode found at: %d\n", free);
			foundFree = 1;
		}
		i++;
	}
	

	if(foundFree == 0)
	{
		printf("\tDEBUG: minode array full! iget failed.\n");
		printf("-iget: \n");
		return NULL;
	}


	printf("\tDEBUG: minode not found. loading new minode...\n");
	MINODE *mip = &minode[free];  // get a pointer to the next free block in the MINODE array
	
	// get inode pointer from inode table using mailman's algorithm
	extern int inode_start;
	int block = (ino - 1) / 8 + inode_start;  // use mailman's algorithm to get block
	int offset = (ino - 1) % 8;  // use mailman's algorithm to get offset
	char buf[BLKSIZE];
	get_block(dev_ref, block, buf);
	ip = (INODE *)buf + offset;
	
	// initialize all the variables in MINODE[free]
	memcpy(&mip->INODE, ip, sizeof(INODE));
	mip->dev = dev_ref;
	mip->ino = ino;
	mip->refCount++;
	//	TODO: figure out what these should be 
	mip->dirty = 0;
	mip->mounted = 0;
	mip->mountptr = 0;
	 
	printf("-iget: \n");
	return &minode[free];  
}



int m_getino(int *dev_ref, char *pathname)
{
	printf("+getino: \n");
	int ino;
	extern PROC *running;
	extern MINODE *root;
	extern char names[64][64];
	extern char *name[64];
	extern int n;
	char buf[BLKSIZE];
	dp = (DIR *)buf;
	char *cp = buf;
	

	// get root ino if / is entered
	if(strcmp(pathname, "/") == 0)
	{
		ino = root->ino;
		dev_ref = root->dev;
		//printf("\tDEBUG: root ino = %d\n", ino);
		printf("-getino: \n");
		return ino;
	}

	m_tokenize(pathname); // tokenize the pathname

	// if the pathname starts with / begin at the root directory
	if(pathname[0] == '/')
	{
		ino = root->ino; // get root ino
		dev_ref = root->dev;
		//printf("\tDEBUG: pathname begins at root ino: %d\n", ino);
	}

	// otherwise start at cwd
	else
	{
		ino = running->cwd->ino;
		dev_ref = running->cwd->dev;
		//printf("\tDEBUG: pathname begins at cwd, ino: %d\n", ino);
	}

	// starting from the ino we just found, search for each dir name
	int i;
	MINODE *mip;
	printf("DEBUG: number of tokens: %d\n", n);
	for(i = 0; i < n; i++)
	{
		mip = m_iget(dev_ref, ino);	
/*
    if (mip->mounted == 1) // has been mounted on
    {
				printf("DEBUG: mounted thing used\n");
        dev_ref = mip->mountptr->dev;
        ino = m_iget(dev_ref, ROOT_INODE);
		    mip = m_iget(dev_ref, ino);
    }	
*/

		printf("DEBUG: about to search name[i] %s\n", name[i]);
		ino = m_search(mip, name[i]);
		if(ino == 0)  // check to see if directory was found
		{
			//printf("\tDEBUG: name not found. returning 0.\n");
			printf("-getino: \n");
			
			return 0;
		}
		printf("\tDEBUG: name[%d] = %s, ino = %d\n", i, name[i], ino);
	}
	printf("-getino: \n");
	return ino;
}

// Helper functions for ialloc
int tst_bit(char *buf, int bit)
{
  int i, j;
  i = bit / 8;  j = bit % 8;
  if (buf[i] & (1 << j))
     return 1;
  return 0;
}

int set_bit(char *buf, int bit)
{
  int i, j;
  i = bit/8; j=bit%8;
  buf[i] |= (1 << j);
}

int decFreeInodes(int dev_ref)
{
  char buf[BLKSIZE];

  // decrement free inodes count in SUPER and GD
  get_block(dev_ref, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count--;
  put_block(dev_ref, 1, buf);

  get_block(dev_ref, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_inodes_count--;
  put_block(dev_ref, 2, buf);
}

int decFreeBlocks(int dev_ref)
{
  char buf[BLKSIZE];

  // dec free blocks count in SUPER and GD
  get_block(dev_ref, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_blocks_count--;
  put_block(dev_ref, 1, buf);

  get_block(dev_ref, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_blocks_count--;
  put_block(dev_ref, 2, buf);
}

int ialloc(int dev_ref)
{
	int  i;
  char buf[BLKSIZE];
	extern int ninodes;
	extern int imap;

	//int dev = running->cwd->dev;
  get_block(dev_ref, imap, buf); // set buf to imap

	printf("n inodes: %d\n", ninodes);
  for (i=0; i < ninodes; i++){
		// If there's a free inode..
		if (tst_bit(buf, i)==0){  
			set_bit(buf, i); // Set the inode to 1, so we know its used
			decFreeInodes(dev_ref); // Decrement free inodes

			put_block(dev_ref, imap, buf); //  Writes buf into the device
			
			printf("free inode number: %i\n", i+1);
			return i+1;
    }
  }
  printf("ialloc(): no more free inodes\n");
  return 0;
}

int balloc(int dev_ref)
{
	int  i;
  char buf[BLKSIZE];
	extern int nblocks;
	extern int bmap;

  // read inode_bitmap block
  get_block(dev_ref, bmap, buf); // Set buf to bmap

  for (i=0; i < nblocks; i++){
		// If there's a free block..
		if (tst_bit(buf, i)==0){	
			set_bit(buf, i);	// Set the inode to 1, so we know its used
			decFreeBlocks(dev_ref); // Decrement free blocks

			put_block(dev_ref, bmap, buf); // Writes the buf into the device
			
			printf("free block number: %i\n", i+1);
			return i+1;
    }
  }
  printf("balloc(): no more free blocks\n");
  return 0;
}


