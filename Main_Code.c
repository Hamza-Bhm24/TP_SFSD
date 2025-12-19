#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAXTAB 100


//////////////////TOF///////////////////////////////////////
// type of records (adapt to the using context)
typedef struct rec {
        int from_id;
        int to_id;
        int rating;
        int timestamp;
        } t_rec;

// type of a data block (and therefore of buffer variables as well)
typedef struct blck {
	t_rec tab[MAXTAB]; // array of records inside a block
	char del[MAXTAB]; // logical erase indicators ('*' erased / ' ' not erased)
	int nb; // number of records inserted in the bloc
	long link;
} t_block;


// the file header (characteristics)
typedef struct header_Tof {
	   long nBlock; // number of blocks in the file (this is also the number of the last block)
	   long nIns;   // number of records in the file
	   long nDel;   // number of records deleted (logically) from file
	} t_header;


typedef struct hdr_lnof {
	long blck_nb;
	long rec_nb;
	long newblck;    // the file border (the first free block available to expand the file size)
} l_header;

typedef struct LnOFstr {
	FILE *l;
	l_header h;
}l_LnOF;

// TOF file structure
typedef struct TOFstr {
            FILE *f;    // C stream implementing the file
            t_header h; // the header in main memory
        } t_TOF;

/**********************************************************
 * Implementation of TOF module functions (TOF_model.c)   *
 * SFSD (File & Data Structures) / 2CP / ESI / 2024       *
 **********************************************************/

void search(t_rec record, bool in_overflow, bool *inside_overflow,
			bool *found, long *i, long *j);

void internal_shift(t_block *buf, long j);


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// open a TOF file / mode ='N' for a New file and mode ='E' for an Existing file
// returns a pointer to a newly allocated variable of type 't_TOF'
void TOF_open( t_TOF **F, char *fname, char mode )
{
   *F = malloc( sizeof( t_TOF) );
   if ( mode == 'E' || mode == 'e' ) {
        // openning an existing TOF file ...
	(*F)->f = fopen( fname, "rb+" );
	if ( (*F)->f == NULL ) {
           perror( "TOF_open" );
           exit( EXIT_FAILURE );
        }
        // loading header part in main memory (in (*F)->h)
	fread( &((*F)->h), sizeof(t_header), 1, (*F)->f );
   }
   else { // mode == 'N' || mode == 'n'
        // creating a new TOF file ...
	(*F)->f = fopen( fname, "wb+" );
	if ( (*F)->f == NULL ) {
           perror( "TOF_open" );
           exit( EXIT_FAILURE );
        }
        // initializing the header part in main memory (in (*)->h)
	(*F)->h.nBlock = 0;
	(*F)->h.nIns = 0;
	(*F)->h.nDel = 0;
	fwrite( &((*F)->h), sizeof(t_header), 1, (*F)->f );
   }
}


// close a TOF file :
// the header is first saved at the beginning of the file and the t_TOF variable is freed
void TOF_close( t_TOF *F )
{
   // saving header part in secondary memory (at the begining of the stream F->f)
   fseek(F->f, 0L, SEEK_SET);
   fwrite( &F->h, sizeof(t_header), 1, F->f );
   fclose(F->f);
   free( F );
}


// reading data block number i into variable buf
void TOF_readBlock( t_TOF *F, long i, t_block *buf )
{
   fseek(F->f, sizeof(t_header) + (i-1)*sizeof(t_block), SEEK_SET);
   fread( buf, sizeof(t_block), 1, F->f );
}


// writing the contents of the variable buf in data block number i
void TOF_writeBlock( t_TOF *F, long i, t_block *buf )
{
   fseek(F->f, sizeof(t_header) + (i-1)*sizeof(t_block), SEEK_SET);
   fwrite( buf, sizeof(t_block), 1, F->f );
}


// header modification
void setHeader( t_TOF *F , char *hname , long val )
{
   if ( strcmp( hname , "nBlock" ) == 0 ) { F->h.nBlock = val; return; }
   if ( strcmp( hname , "nIns" ) == 0 ) { F->h.nIns = val; return; }
   if ( strcmp( hname , "nDel" ) == 0 ) { F->h.nDel = val; return; }
   fprintf(stderr, "setHeader : Unknown headerName: \"%s\"\n", hname);
}


// header value
long getHeader( t_TOF *F , char * hname )
{
   if ( strcmp( hname , "nBlock" ) == 0 ) return F->h.nBlock;
   if ( strcmp( hname , "nIns" ) == 0 ) return F->h.nIns;
   if ( strcmp( hname , "nDel" ) == 0 ) return F->h.nDel;
   fprintf(stderr, "getHeader : Unknown headerName: \"%s\"\n", hname);
}





void LnOF_open( l_LnOF **L, char *fname, char mode )
{
   t_block bufl;

   *L = malloc( sizeof( l_LnOF) );

   if ( mode == 'E' || mode == 'e' ) {
       // openning an existing LnOF file ...
       (*L)->l = fopen( fname, "rb+" );
       if ( (*L)->l == NULL ) {
           perror( "LnOVS_open" );
           exit( EXIT_FAILURE );
       }
       // loading header part in main memory (in (*F)->h)
       fread( &((*L)->h), sizeof(l_header), 1, (*L)->l );
   }
   else {
       // creating a new LnOVS file ...
       (*L)->l = fopen( fname, "wb+" );
       if ( (*L)->l == NULL ) {
           perror( "LnOF_open" );
           exit( EXIT_FAILURE );
       }
       // initializing the header part in main memory (in (*F)->h)
       (*L)->h.newblck = 1;
       (*L)->h.blck_nb = 1; // initially the list of freed blocks is empty
   	   (*L)->h.rec_nb = 0;
       // writing the headers at offset 0 of stream (*F)->f
       fwrite( &((*L)->h), sizeof(l_header), 1, (*L)->l );
       // writing the first allocated block
       bufl.link = -1;
       fwrite( &bufl, sizeof(t_block), 1, (*L)->l );
   }
}  // LnOVS_open


// close a LnOVS file :
// the header is first saved at the beginning of the file (offset 0) and the t_LnOVS variable is freed
void LnOF_close( l_LnOF *L )
{
   // saving header part in secondary memory (at the begining of the stream F->f)
   fseek(L->l, 0L, SEEK_SET);
   fwrite( &L->h, sizeof(l_header), 1, L->l );
   fclose(L->l);
   free( L );
}


// reading data block number i into variable buf
void LnOF_readBlock( l_LnOF *L, long i, t_block *bufl )
{
   fseek(L->l, sizeof(l_header) + (i-1)*sizeof(t_block), SEEK_SET);
   fread( bufl, sizeof(t_block), 1, L->l );
}


// writing the contents of the variable buf in data block number i
void LnOF_writeBlock( l_LnOF *L, long i, t_block *bufl )
{
   fseek(L->l, sizeof(l_header) + (i-1)*sizeof(t_block), SEEK_SET);
   fwrite( bufl, sizeof(t_block), 1, L->l );
}


// header updates in main memory
void setHeaderL( l_LnOF *L , char *hname , long val )
{
   if ( strcmp( hname , "blck_nb" ) == 0 ) { L->h.blck_nb = val; return; }
   if ( strcmp( hname , "newblck" ) == 0 ) { L->h.newblck = val; return; }
   if ( strcmp( hname , "rec_nb" ) == 0 ) { L->h.rec_nb = val; return; }
   fprintf(stderr, "setHeader : Unknown headerName: \"%s\"\n", hname);
}


// header values (from main memory)
long getHeaderL( l_LnOF *L , char * hname )
{
   if ( strcmp( hname , "blck_nb" ) == 0 ) return L->h.blck_nb;
   if ( strcmp( hname , "newblck" ) == 0 ) return L->h.newblck;
   if ( strcmp( hname , "rec_nb" ) == 0 ) return L->h.rec_nb;
   fprintf(stderr, "getHeader : Unknown headerName: \"%s\"\n", hname);
}


// allocate a new block to the file
long LnOF_allocBlock( l_LnOF *L )
{
   long i;
   t_block bufl;

      i = L->h.newblck;
      L->h.newblck++;

	return i;
}


// mark block i as unused
//void LnOF_freeBlock( l_LnOF *L, long i )
//{
  // t_block buf;

   // add i to the list of freed blocks (insertion at the beginning of the ist) ...
   //buf.link = L->h.freeblck;
   //LnOVS_writeBlock( L , i , &buf );
   //L->h.freeblck = i;          // the new head of the free-blocks list is now i
//}




// the TOF file
t_TOF *f = NULL;
l_LnOF *l =NULL;

t_block buf;// the buffer in main memory
t_block bufl;
long block_in_MM = -1;    // the number of the last block read (in buf)

// the TOF access operations

void swap(t_rec *a, t_rec *b);
void buble_sort(t_rec bulk_table[]);


void fill_table(t_rec bulk_table[], t_rec insertion_table[])
{
   long i, k, n;
   int j,total_rec;
   FILE *tp;
   char line[300],line_copy[300];
   char *feild;

   tp = fopen("D:\\Downloads\\Dutch_college_social_network_randomized.txt","r");
   total_rec=1530;
   printf("Bulk Loading\n");

   j = 0;
   i = 1;
   for (k=0; k<total_rec; k++) {
        fgets(line, sizeof(line), tp);
        // Copy line to avoid modifying the original
        strcpy(line_copy, line);

        // Extract fields using strtok
        feild = strtok(line_copy, " ");

        if (feild) bulk_table[j].from_id = atoi(feild);

        feild = strtok(NULL, " ");
        if (feild) bulk_table[j].to_id = atoi(feild);


        feild = strtok(NULL, " ");
        if (feild) bulk_table[j].rating = atoi(feild);

        feild = strtok(NULL, " ");
        if (feild) bulk_table[j].timestamp = atoi(feild);
	   j++;
	}
	j=0;
	while (fgets(line,sizeof(line),tp)) {
		// Copy line to avoid modifying the original
		strcpy(line_copy, line);

		// Extract fields using strtok
		feild = strtok(line_copy, " ");

		if (feild) insertion_table[j].from_id = atoi(feild);

		feild = strtok(NULL, " ");
		if (feild) insertion_table[j].to_id = atoi(feild);


		feild = strtok(NULL, " ");
		if (feild) insertion_table[j].rating = atoi(feild);

		feild = strtok(NULL, " ");
		if (feild) insertion_table[j].timestamp = atoi(feild);
		j++;
	}
	printf("number of inserted in second table is:%d",j);
   fclose(tp);
   buble_sort(bulk_table);
}

void buble_sort(t_rec bulk_table[]) {
    int n = 1530; // number of records
    bool sorted;

    while (!sorted) {
        sorted = true;
        for (int i = 0; i < n - 1; i++) {     // <= FIXED
            if (bulk_table[i].from_id > bulk_table[i + 1].from_id ||
                (bulk_table[i].from_id == bulk_table[i + 1].from_id &&
                 bulk_table[i].to_id > bulk_table[i + 1].to_id) ||
                (bulk_table[i].from_id == bulk_table[i + 1].from_id &&
                 bulk_table[i].to_id == bulk_table[i + 1].to_id &&
                 bulk_table[i].timestamp > bulk_table[i + 1].timestamp)) {

                swap(&bulk_table[i], &bulk_table[i + 1]);
                sorted = false;
            }
        }
    }
}


void swap(t_rec *a,t_rec *b){
    t_rec c;
    c=*a;
    *a=*b;
    *b=c;
}
void table_display(t_rec *bulk_table) {
	for (int i = 0; i < 1530; i++) {
		printf("%d ",bulk_table[i].from_id);
		printf("%d ",bulk_table[i].to_id);
		printf("%d ",bulk_table[i].rating);
		printf("%d ",bulk_table[i].timestamp);
		printf("\n");
	}
}

void bulk_load(t_rec bulk_table[])
{
   long i, k;
   int j,total_rec;

   total_rec=1530;

   printf("Bulk Loading\n");

   j = 0;
   i = 1;
   for (k=0; k<total_rec; k++) {
	if ( j < MAXTAB*0.75 ) {
        buf.tab[j].from_id = bulk_table[k].from_id;
        buf.tab[j].to_id = bulk_table[k].to_id;
        buf.tab[j].rating = bulk_table[k].rating;
        buf.tab[j].timestamp = bulk_table[k].timestamp;
	    buf.del[j] = ' ';
	   j++;
	}
	else {
	   buf.nb = j;
		buf.link=-1;
	   TOF_writeBlock( f, i, &buf );
	   i++;
       buf.tab[0].from_id = bulk_table[k].from_id;
       buf.tab[0].to_id = bulk_table[k].to_id;
       buf.tab[0].rating = bulk_table[k].rating;
       buf.tab[0].timestamp = bulk_table[k].timestamp;
	   buf.del[0] = ' ';
	   j = 1;
	}
   }
   // last write ...
   buf.nb = j;
   TOF_writeBlock( f, i, &buf );
   // headers update ...
   setHeader( f , "nBlock" , i );  // number of block used
   setHeader( f , "nIns" , total_rec );    // number of records inserted
}


// display headers info ...
void info()
{
	printf("Headers \n");
	printf("\tNumber of blocks = %ld\n", getHeader( f, "nBlock" ) );
	printf("\tNumber of inserted records = %ld\n", getHeader( f, "nIns" ));
	printf("\tNumber of deleted records = %ld\n", getHeader( f, "nDel" ));
	printf("\tLoading factor = %ld %%\n", \
	 (getHeader( f, "nBlock" ) == 0 ? 0 : \
		   (long)(((double)getHeader( f, "nIns" ) / (getHeader( f, "nBlock" )*MAXTAB))*100)) );
} // info

void range()
{
	long i, a, b;
	int j;

	printf("Displaying the contents of the contiguous block sequence between block a and block b (a <= b) \n");
	printf("currently the file starts in block number 1 and ends in block number %ld\n", getHeader(f, "nBlock") );
	for (i = 1; i <= 21; i++) {
		TOF_readBlock( f, i, &buf );
		block_in_MM = i;
		printf("[Block:%3ld \tNB = %2d \tMaxCapacity = %2d]\n", i, buf.nb, MAXTAB);
		for( j=0; j<buf.nb; j++)
			if ( buf.del[j] == ' ' ){
				printf("%ld ", buf.tab[j].from_id);
				printf("%ld ", buf.tab[j].to_id);
				printf("%ld ", buf.tab[j].rating);
				printf("%ld ", buf.tab[j].timestamp);
		printf("\n--------------------------------------------------\n");
			}
	}

} // range


void insertion(t_rec insertion_table[],long *k) {
    long N;
    bool found1, inside = false;
    long p, i, j, current;
    t_block new_block;
    t_rec new_rec;

    printf("How many record you want to insert? \n");
    scanf("%d", &N);
    printf("Insertion of %d record...\n", N);
	N=N+(*k);
    for (*k; *k < N; (*k)++) {
        t_block local_buf;  // Local buffer for the current block
        printf("Insertion of record : %d %d %d %d\n", insertion_table[*k].from_id,
               insertion_table[*k].to_id, insertion_table[*k].rating, insertion_table[*k].timestamp);
        new_rec = insertion_table[*k];
        search(new_rec, false, &inside, &found1, &i, &j);
        TOF_readBlock(f, i, &local_buf);
        if (local_buf.nb == 100) {
            if (local_buf.link == -1) {
                p = getHeaderL(l, "newblck");
            	setHeaderL(l, "newblck", p+1);
                bufl.tab[0].from_id = insertion_table[*k].from_id;
                bufl.tab[0].to_id = insertion_table[*k].to_id;
                bufl.tab[0].rating = insertion_table[*k].rating;
                bufl.tab[0].timestamp = insertion_table[*k].timestamp;
                bufl.del[0] = ' ';
                bufl.nb = 1;
                bufl.link = -1;
                LnOF_writeBlock(l, p, &bufl);
                local_buf.link = p;
                TOF_writeBlock(f, i, &local_buf);
            	printf("DEBUG: Record inserted into NEW overflow block %ld at position 0\n", p);
            } else {
            	current = local_buf.link;
            	LnOF_readBlock(l, current, &bufl);

            	while (bufl.link != -1) {
            		current = bufl.link;
            		LnOF_readBlock(l, current, &bufl);
            	}

                if (bufl.nb == 100) {
                    p = LnOF_allocBlock(l);
                    bufl.link = p;
                    LnOF_writeBlock(l, current, &bufl);
                    new_block.tab[0].from_id = insertion_table[*k].from_id;
                    new_block.tab[0].to_id = insertion_table[*k].to_id;
                    new_block.tab[0].rating = insertion_table[*k].rating;
                    new_block.tab[0].timestamp = insertion_table[*k].timestamp;
                    new_block.del[0] = ' ';
                    new_block.nb = 1;
                    new_block.link = -1;
                    LnOF_writeBlock(l, p, &new_block);
                    TOF_writeBlock(f, i, &local_buf);  // Ensure the TOF block is written back if needed
                	printf("DEBUG: Record inserted into NEW overflow block %ld at position 0\n", p);
                } else {
                    bufl.tab[bufl.nb].from_id = insertion_table[*k].from_id;
                    bufl.tab[bufl.nb].to_id = insertion_table[*k].to_id;
                    bufl.tab[bufl.nb].rating = insertion_table[*k].rating;
                    bufl.tab[bufl.nb].timestamp = insertion_table[*k].timestamp;
                    bufl.del[bufl.nb] = ' ';
                    bufl.nb++;
                    LnOF_writeBlock(l, current, &bufl);  // Write back the updated overflow block
                	printf("DEBUG: Record inserted into EXISTING overflow block %ld at position %d\n", current, bufl.nb - 1);
                }
            }
        } else {
            internal_shift(&local_buf, j);
            local_buf.tab[j].from_id = insertion_table[*k].from_id;
            local_buf.tab[j].to_id = insertion_table[*k].to_id;
            local_buf.tab[j].rating = insertion_table[*k].rating;
            local_buf.tab[j].timestamp = insertion_table[*k].timestamp;
            local_buf.del[j] = ' ';
            local_buf.nb++;
            TOF_writeBlock(f, i, &local_buf);
        	printf("DEBUG: Record inserted into TOF block %ld at position %ld\n", i, j);
        }
        printf("Insertion %ld Done\n", (*k) + 1);  // Use %ld for long
    }

}


void internal_shift(t_block *buf, long j)
{
	for (int i = buf->nb; i > j; i--) {
		buf->tab[i] = buf->tab[i - 1];
	}
}

int cmp_records(t_rec a, t_rec b) {
	if (a.from_id != b.from_id) return a.from_id - b.from_id;
	if (a.to_id != b.to_id) return a.to_id - b.to_id;
	return a.timestamp - b.timestamp;
}

void search(t_rec record,bool in_overflow,bool *inside_overflow,bool *found,long *i,long *j)
{
	long min,max,inf,sup,mid_block,mid_val,last_block;
	bool block_chosen=false;
	min=1;
	max=getHeader(f,"nBlock");
	*found=false;
	*inside_overflow=false;
	while (!(*found) && min <= max){
		//searching for the right block
		mid_block=(max+min)/2;
		TOF_readBlock(f,mid_block,&buf);
		if ((record.from_id < buf.tab[0].from_id) ||
		(record.from_id == buf.tab[0].from_id && record.to_id < buf.tab[0].to_id) ||
		(record.from_id == buf.tab[0].from_id && record.to_id == buf.tab[0].to_id && record.timestamp < buf.tab[0].timestamp))
		{
			max = mid_block - 1;
		}
		// If record > last record of block
		else{
			if ((record.from_id > buf.tab[buf.nb-1].from_id) ||
				 (record.from_id == buf.tab[buf.nb-1].from_id && record.to_id > buf.tab[buf.nb-1].to_id) ||
				 (record.from_id == buf.tab[buf.nb-1].from_id && record.to_id == buf.tab[buf.nb-1].to_id && record.timestamp > buf.tab[buf.nb-1].timestamp))
			{
			min = mid_block + 1;
			}
			else { //internal search
				inf=0;
				sup=buf.nb-1;
				while (!(*found) && inf<=sup) {
					mid_val=(sup+inf)/2;
					if (record.from_id == buf.tab[mid_val].from_id &&
						record.to_id   == buf.tab[mid_val].to_id &&
						record.timestamp == buf.tab[mid_val].timestamp)
					{
						*found=true;
						*j=mid_val;
					}
					else{
						if (record.from_id<buf.tab[mid_val].from_id) {
							sup=mid_val-1;
						}
						else {
							if (record.from_id>buf.tab[mid_val].from_id) {
								inf=mid_val+1;
							}
							else {
								if (record.to_id<buf.tab[mid_val].to_id) {
									sup=mid_val-1;
								}
								else {
									if (record.to_id>buf.tab[mid_val].to_id) {
										inf=mid_val+1;
									}
									else {
										if (record.timestamp<buf.tab[mid_val].timestamp) {
											sup=mid_val-1;
										}
										else {
											if (record.timestamp>buf.tab[mid_val].timestamp) {
												inf=mid_val+1;
											}
										}
									}
								}
							}
						}
					}
				}
				if (inf>sup) *j=inf;
				else
					*j=mid_val;
				*i = mid_block;
				block_chosen = true;

				break;
			}
		}
	}
	if (!(*found) && !block_chosen) {
		// min and max are the binary search bounds
		if (max < 1) *i = 1;  // insert at first block
		else {
			if (min > getHeader(f, "nBlock")) {
				*i = getHeader(f, "nBlock");  // insert at last block
			} else {
				*i = max;  // insert in the last block smaller than new record
			}
		}
	}

	if (in_overflow == true && !(*found)) {  // Search overflow of the correct block (*i)
		TOF_readBlock(f, *i, &buf);  // <-- FIX: Read the correct block into buf
		*i = buf.link;  // Start from its overflow chain
		while (*i != -1 && !(*found)) {
			LnOF_readBlock(l, *i, &bufl);
			for (int pos = 0; pos < bufl.nb; pos++) {
				if (bufl.tab[pos].from_id == record.from_id &&
					bufl.tab[pos].to_id == record.to_id &&
					bufl.tab[pos].timestamp == record.timestamp) {
					*found = true;
					*inside_overflow = true;
					*j = pos;  // Position in overflow
					break;
					}
			}
			last_block=*i;
			*i = bufl.link;  // Move to next overflow block
		}
		*i=last_block;
	}
}


void display_overflow_of_block(long tof_index)
{
	t_block buf_tof;
	t_block buf_of;
	long of_index;

	TOF_readBlock(f, tof_index, &buf_tof);

	printf("\nTOF Block %ld (nb=%d, link=%ld)\n",
		   tof_index, buf_tof.nb, buf_tof.link);

	of_index = buf_tof.link;

	if (of_index == -1) {
		printf("   No overflow\n");
		return;
	}

	while (of_index != -1) {
		LnOF_readBlock(l, of_index, &buf_of);

		printf("   Overflow Block %ld (nb=%d, link=%ld)\n",
			   of_index, buf_of.nb, buf_of.link);

		for (int i = 0; i < buf_of.nb; i++) {
			printf("      [%d] (%d, %d, %d)\n",
				   i,
				   buf_of.tab[i].from_id,
				   buf_of.tab[i].to_id,
				   buf_of.tab[i].timestamp);
		}

		of_index = buf_of.link;
	}
}

void display_all_overflows()
{
	long nblocks = getHeader(f, "nBlock");

	printf("\n======= OVERFLOW ZONES =======\n");

	for (long i = 1; i <= nblocks; i++) {
		display_overflow_of_block(i);
	}

	printf("======= END OVERFLOWS =======\n");
}






int main()
{
   int choice;
	long i,j,k=0;
	bool found,inside_overflow;
   char name[20], mode[20];
   t_rec bulk_table[2000];
	t_rec insertion_table[2000];
	t_rec search_record;

   printf("Access operations on a TOF type file\n");
   printf("Maximum block capacity = %d records\t", MAXTAB);
   printf("Block size = %ld \tHeader size = %ld\n\n", sizeof(t_block), sizeof(t_header) );

   // Ouverture du fichier ...
   printf("File name to open/create : ");
   scanf(" %s", name);
   printf("openning an 'E'xisiting file or creating a 'N'ew file ? (E/N) : ");
   scanf(" %s", mode);
   if ( mode[0] == 'e' || mode[0] == 'E' ) {
	   TOF_open( &f, name , 'E' );
   	LnOF_open(&l,"insertion",'E');
   }
   else {
	TOF_open( &f, name , 'N' );
   	LnOF_open(&l,"insertion",'N');
	fill_table(bulk_table,insertion_table);
	bulk_load(bulk_table);
   }

   // Menu principal ...
   do {
      	printf("\n--------- M E N U ( filename = '%s' ) ---------\n", name );
	printf("1) Display header contents\n");
   	printf("2) Display blocks contents\n");
   	printf("3) Insert new records\n");
   	printf("4) Search\n");
   	printf("5) Display overflow zones\n");
   	printf("6) exit\n");
	scanf(" %d", &choice);
	printf("\n---------------------------\n\n");

	switch(choice) {
	   case 1:
		   info();
			break;
		case 2: range(); break;
		case 3: {
				t_block buf = {0};  // ZERO ALL FIELDS
				t_block bufl = {0};
				insertion(insertion_table,&k);
				break;
			}
			case 4: {
			printf("Insert the record to search for...\n");
			printf("From ID: ");
			scanf("%d",&search_record.from_id);
			printf("\n");
			printf("To Id: ");
			scanf("%d",&search_record.to_id);
			printf("\n");
			printf("Rating: ");
			scanf("%d",&search_record.rating);
			printf("\n");
			printf("Timestamp: ");
			scanf("%d",&search_record.timestamp);
			printf("\n");
			search(search_record,true,&inside_overflow,&found,&i,&j);
			if (found && inside_overflow) {
				printf("Record found in:\nOverflow zone\nBlock %d\nPosition %d",i,j);
			}
			if (found && !inside_overflow) {
				printf("Record found in:\nTOF File zone\nBlock %d\nPosition %d",i,j);
			}
			if (!found) {
				printf("Record not found\n");
			}
			break;
		}
		case 5: display_all_overflows();
			break;
			case 6: break;
	}
   } while ( choice != 6);

   // Closing the file (and save headers)
   TOF_close(f);
	LnOF_close(l);

   return 0;

} // main



