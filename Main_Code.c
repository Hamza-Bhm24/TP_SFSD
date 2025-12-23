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

// LnOF file structure
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

void search(t_rec record, bool in_overflow, bool *inside_overflow,bool *found, long *i, long *j);

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
       (*L)->h.blck_nb = 0; // initially the list of freed blocks is empty
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
   total_rec=1531;

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
   fclose(tp);
   buble_sort(bulk_table);
}

//int get_rec_nb(FILE *tp) {
  //  char line[200];
    //int count = 0;

   // rewind(f);  // go to beginning of file

    //while (fgets(line, sizeof(line), tp) != NULL) {
      //  count++;
    //}

    //return count;
//}


void buble_sort(t_rec bulk_table[]) {
    int n = 1531; // number of records
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

   total_rec=1531;

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
void info_TOF()
{
	printf("Headers \n");
	printf("\tNumber of blocks = %ld\n", getHeader( f, "nBlock" ) );
	printf("\tNumber of inserted records = %ld\n", getHeader( f, "nIns" ));
	printf("\tNumber of deleted records = %ld\n", getHeader( f, "nDel" ));
	printf("\tLoading factor = %ld %%\n", \
	 (getHeader( f, "nBlock" ) == 0 ? 0 : \
		   (long)(((double)getHeader( f, "nIns" ) / (getHeader( f, "nBlock" )*MAXTAB))*100)) );
} // info

void info_LnOF()
{
	printf("Headers \n");
	printf("\tNumber of blocks = %ld\n", getHeaderL( l, "blck_nb" ) );
	printf("\tNumber of inserted records = %ld\n", getHeaderL( l, "rec_nb" ));
} // info

void range(bool only_one)
{
	long i,b;
	int j;

    if(!only_one){
        printf("ALL TOF Blocks\n");
        for (i = 1; i <= getHeader(f,"nBlock"); i++) {
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
    }
    else{
        printf("Block are from 1 to %d\n",getHeader(f,"nBlock"));
        printf("Enter the number of block that you want to display: ");
        scanf("%d",&b);
        while(b>getHeader(f,"nBlock") || b<1){
            printf("Enter a valide block number!!\n");
            printf("Block: ");
            scanf("%d",&b);
        }
		TOF_readBlock( f, b, &buf );
		printf("[Block:%3ld \tNB = %2d \tMaxCapacity = %2d]\n", b, buf.nb, MAXTAB);
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

void insertion(t_rec insertion_table[],long *k,long *rec_remained) {
    long N;
    bool found1, inside = false;
    long p, i, j, current;
    t_block new_block;
    t_rec new_rec;

    if(*rec_remained==0){
        printf("You have inserted all available records\n");
    }
    else{
    printf("There is %d records remained\n",*rec_remained);
    printf("How many record you want to insert? \n");
    scanf("%d", &N);
    while(N>(*rec_remained)){
        printf("Insert a number between 1 and %d\n",*rec_remained);
        scanf("%d", &N);
    }
    printf("Insertion of %d record...\n", N);

	N=N+(*k);

    for (*k; *k < N; (*k)++) {
        t_block local_buf;
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
                setHeaderL(l,"blck_nb",getHeaderL(l,"blck_nb")+1);
                setHeaderL(l,"rec_nb",getHeaderL(l,"rec_nb")+1);
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
                    setHeaderL(l,"blck_nb",getHeaderL(l,"blck_nb")+1);
                    setHeaderL(l,"rec_nb",getHeaderL(l,"rec_nb")+1);
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
                    setHeaderL(l,"rec_nb",getHeaderL(l,"rec_nb")+1);
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
            setHeader(f,"nIns",getHeader(f,"nIns")+1);
            TOF_writeBlock(f, i, &local_buf);
        	printf("DEBUG: Record inserted into TOF block %ld at position %ld\n", i, j);
        }
        printf("Insertion %ld Done\n", (*k) + 1);  // Use %ld for long
        (*rec_remained)--;
    }
    }

}


void internal_shift(t_block *local_buf, long j)
{
	for (int i = local_buf->nb; i > j; i--) {
		local_buf->tab[i] = local_buf->tab[i - 1];
	}
}

int cmp_records(t_rec a, t_rec b) {
	if (a.from_id != b.from_id) return a.from_id - b.from_id;
	if (a.to_id != b.to_id) return a.to_id - b.to_id;
	return a.timestamp - b.timestamp;
}

void search(t_rec record, bool in_overflow, bool *inside_overflow, bool *found, long *i, long *j) {
    long min, max, inf, sup, mid_block, mid_val, last_block;
    bool block_chosen = false;
    min = 1;
    max = getHeader(f, "nBlock");
    *found = false;
    *inside_overflow = false;

    while (!(*found) && min <= max) {
        // Searching for the right block
        mid_block = (max + min) / 2;
        TOF_readBlock(f, mid_block, &buf);
        if ((record.from_id < buf.tab[0].from_id) ||
            (record.from_id == buf.tab[0].from_id && record.to_id < buf.tab[0].to_id) ||
            (record.from_id == buf.tab[0].from_id && record.to_id == buf.tab[0].to_id && record.timestamp < buf.tab[0].timestamp)) {
            max = mid_block - 1;
        } else {
            if ((record.from_id > buf.tab[buf.nb - 1].from_id) ||
                (record.from_id == buf.tab[buf.nb - 1].from_id && record.to_id > buf.tab[buf.nb - 1].to_id) ||
                (record.from_id == buf.tab[buf.nb - 1].from_id && record.to_id == buf.tab[buf.nb - 1].to_id && record.timestamp > buf.tab[buf.nb - 1].timestamp)) {
                min = mid_block + 1;
            } else {  // Internal search
                inf = 0;
                sup = buf.nb - 1;
                while (!(*found) && inf <= sup) {
                    mid_val = (sup + inf) / 2;
                    if (record.from_id == buf.tab[mid_val].from_id &&
                        record.to_id == buf.tab[mid_val].to_id &&
                        record.timestamp == buf.tab[mid_val].timestamp) {
                        *found = true;
                        *j = mid_val;
                    } else {
                        if (record.from_id < buf.tab[mid_val].from_id ||
                            (record.from_id == buf.tab[mid_val].from_id && record.to_id < buf.tab[mid_val].to_id) ||
                            (record.from_id == buf.tab[mid_val].from_id && record.to_id == buf.tab[mid_val].to_id && record.timestamp < buf.tab[mid_val].timestamp)) {
                            sup = mid_val - 1;
                        } else {
                            inf = mid_val + 1;
                        }
                    }
                }
                if (!(*found)) *j = inf;
                *i = mid_block;
                block_chosen = true;
                break;
            }
        }
    }

    if (!(*found) && !block_chosen) {
        // Determine *i
        if (max < 1) *i = 1;
        else if (min > getHeader(f, "nBlock")) *i = getHeader(f, "nBlock");
        else *i = max;

        // Now set *j for block *i (buf is already loaded from the last TOF_readBlock)
        TOF_readBlock(f, *i, &buf);  // Ensure buf is the correct block
        inf = 0;
        sup = buf.nb - 1;
        while (inf <= sup) {
            mid_val = (sup + inf) / 2;
            int cmp = cmp_records(record, buf.tab[mid_val]);
            if (cmp == 0) {
                *found = true;
                *j = mid_val;
                break;
            } else if (cmp < 0) {
                sup = mid_val - 1;
            } else {
                inf = mid_val + 1;
            }
        }
        if (!(*found)) *j = inf;
    }

    if (in_overflow == true && !(*found)) {  // Search overflow of the correct block (*i)
        TOF_readBlock(f, *i, &buf);
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
            last_block = *i;
            *i = bufl.link;  // Move to next overflow block
        }
        *i = last_block;
    }
}




void search_rating() {
    int id1, id2, cas = 0, max, min, mid_block, k, swap;
    bool found = false;

    printf("Enter the id of first student: ");
    scanf("%d", &id1);
    printf("Enter the id of second student: ");
    scanf("%d", &id2);

    while (cas < 2) {
        min = 1;
        max = getHeader(f, "nBlock");

        while (min <= max) {
            mid_block = (min + max) / 2;
            TOF_readBlock(f, mid_block, &buf);

            if (id1 < buf.tab[0].from_id ||
               (id1 == buf.tab[0].from_id && id2 < buf.tab[0].to_id)) {
                max = mid_block - 1;
            }
            else if (id1 > buf.tab[buf.nb - 1].from_id ||
                    (id1 == buf.tab[buf.nb - 1].from_id &&
                     id2 > buf.tab[buf.nb - 1].to_id)) {
                min = mid_block + 1;
            }
            else{
                for (int blk = mid_block-1; blk <= mid_block+1; blk++) { //scan the neighbor block of the mid block
                    if (blk < 1 || blk > getHeader(f,"nBlock")) continue;
                    TOF_readBlock(f, blk, &buf);
                    for(int m=0;m<buf.nb;m++){ //scan the TOF block
                        if(id1==buf.tab[m].from_id && id2==buf.tab[m].to_id){
                            printf("%d %d %d %d\n",
                            buf.tab[m].from_id,
                            buf.tab[m].to_id,
                            buf.tab[m].rating,
                            buf.tab[m].timestamp);
                            found=true;
                        }
                        else
                            if(buf.tab[m].from_id > id1)
                            break;
                    }
                k = buf.link;//scan the overflow
                while (k != -1) {
                    LnOF_readBlock(l, k, &bufl);
                    for (int pos = 0; pos < bufl.nb; pos++) {
                        if (bufl.tab[pos].from_id == id1 && bufl.tab[pos].to_id == id2) {
                            printf("%d %d %d %d\n",
                            bufl.tab[pos].from_id,
                            bufl.tab[pos].to_id,
                            bufl.tab[pos].rating,
                            bufl.tab[pos].timestamp);
                        found=true;
                        }
                    }
                    k = bufl.link;
                }
            }
            break; //stop the binary search
        }

    }

        printf("--------------------------------------------------\n");

        swap = id1;
        id1 = id2;
        id2 = swap;
        cas++;
    }

    if (!found)
        printf("No rating found\n");
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

// Function to update an existing rating
void update_rating() {
    t_rec record_to_update;
    bool found, inside_overflow;
    long i, j;
    int new_rating;

    printf("=== Update an existing rating ===\n");

    // Get the record to update
    printf("Enter From Student ID: ");
    scanf("%d", &record_to_update.from_id);
    printf("Enter To Student ID: ");
    scanf("%d", &record_to_update.to_id);
    printf("Enter Timestamp (to uniquely identify): ");
    scanf("%d", &record_to_update.timestamp);

    // Search for the record
    search(record_to_update, true, &inside_overflow, &found, &i, &j);

    if (!found) {
        printf("Error: Record not found!\n");
        return;
    }

    // Get new rating
    printf("Enter new rating (-1 to 3): ");
    scanf("%d", &new_rating);

    // Validate rating
    if (new_rating < -1 || new_rating > 3) {
        printf("Error: Rating must be between -1 and 3\n");
        return;
    }

    if (inside_overflow) {
        // Record is in overflow file
        LnOF_readBlock(l, i, &bufl);
        bufl.tab[j].rating = new_rating;
        LnOF_writeBlock(l, i, &bufl);
        printf("Rating updated successfully in overflow block %ld at position %ld\n", i, j);
    } else {
        // Record is in TOF file
        TOF_readBlock(f, i, &buf);
        buf.tab[j].rating = new_rating;
        TOF_writeBlock(f, i, &buf);
        printf("Rating updated successfully in TOF block %ld at position %ld\n", i, j);
    }
}

// Function to list all friends of a given student
void list_friends_of_student() {
    int student_id;
    int timestamp;
    int friend_count = 0;

    printf("=== List all friends of a student ===\n");
    printf("Enter Student ID: ");
    scanf("%d", &student_id);
    printf("choose a timestamp (enter the full timestamp) : \n 1-->913330800 \n 2-->915145200 \n 3-->916959600 \n 4-->918774000 \n 5-->920588400 \n 6-->924217200 \n 7-->927846000 \n 8-->000000000 (all time) \n");
    scanf("%d", &timestamp);
    printf("\nFriends of student %d:\n", student_id);
    printf("==============================================\n");
    printf("Friend ID | Rating |  Timestamp  | Where found\n");
    printf("----------|--------| ----------- |------------\n");

    // Search in TOF blocks
    long nblocks = getHeader(f, "nBlock");
    for (long block_num = 1; block_num <= nblocks; block_num++) {
        TOF_readBlock(f, block_num, &buf);

        // Check all records in this TOF block
        for (int pos = 0; pos < buf.nb; pos++) {
            if (buf.del[pos] == ' ' && buf.tab[pos].from_id == student_id && buf.tab[pos].rating >=1 && (buf.tab[pos].timestamp == timestamp || timestamp==000000000) ) {
                printf("%9d | %6d | %8d   | TOF Block %ld\n",
                       buf.tab[pos].to_id,
                       buf.tab[pos].rating,
                       buf.tab[pos].timestamp,
                       block_num);
                friend_count++;
            }
        }

        // Check overflow chain of this TOF block
        long of_block = buf.link;
        while (of_block != -1) {
            LnOF_readBlock(l, of_block, &bufl);

            for (int pos = 0; pos < bufl.nb; pos++) {
                if (bufl.del[pos] == ' ' && bufl.tab[pos].from_id == student_id && bufl.tab[pos].rating >=1 && (bufl.tab[pos].timestamp == timestamp  || timestamp==000000000)) {
                    printf("%9d | %6d | %8d  | Overflow Block %ld\n",
                           bufl.tab[pos].to_id,
                           bufl.tab[pos].rating,
                           bufl.tab[pos].timestamp,
                           of_block);
                    friend_count++;
                }
            }

            of_block = bufl.link;
        }
    }

    printf("========================================\n");
    printf("Total friends found: %d\n", friend_count);

    // Also list students who consider this student as a friend (optional)
    printf("\nStudents who consider student %d as a friend:\n", student_id);
    printf("===============================================\n");
    printf("Friend ID | Rating |  Timestamp  | Where found\n");
    printf("----------|--------| ----------- |------------\n");
    int considered_count = 0;

    for (long block_num = 1; block_num <= nblocks; block_num++) {
        TOF_readBlock(f, block_num, &buf);

        for (int pos = 0; pos < buf.nb; pos++) {
            if (buf.del[pos] == ' ' && buf.tab[pos].to_id == student_id && buf.tab[pos].rating >=1 && (buf.tab[pos].timestamp == timestamp || timestamp==000000000)) {
               printf("%9d | %6d | %8d   | TOF Block %ld\n",
                       buf.tab[pos].from_id,
                       buf.tab[pos].rating,
                        buf.tab[pos].timestamp,
                       block_num);
                considered_count++;
            }
        }

        long of_block = buf.link;
        while (of_block != -1) {
            LnOF_readBlock(l, of_block, &bufl);

            for (int pos = 0; pos < bufl.nb; pos++) {
                if (bufl.del[pos] == ' ' && bufl.tab[pos].to_id == student_id && bufl.tab[pos].rating >=1 && (bufl.tab[pos].timestamp == timestamp || timestamp==000000000)) {
                     printf("%9d | %6d | %8d | Overflow Block %ld\n",
                           bufl.tab[pos].to_id,
                           bufl.tab[pos].rating,
                           bufl.tab[pos].timestamp,
                           of_block);
                    considered_count++;
                }
            }

            of_block = bufl.link;
        }
    }

    printf("Total: %d students\n", considered_count);
}
////////////////////////////////////////////////////////////////////////////////////


// reorganize function
void reorganize_files(char file_name[100]) {
    printf("=== REORGANIZE FILES (Merge TOF and LnOF) ===\n");
    printf("This will merge all active records from TOF and overflow\n");
    printf("into a new sorted TOF file and reset the overflow file.\n");

    printf("\nAre you sure? (1=Yes, 0=No): ");
    int confirm;
    scanf("%d", &confirm);

    if (confirm != 1) {
        printf("Reorganization cancelled.\n");
        return;
    }

    // Create a temporary file for merging
    t_TOF *temp_f = NULL;
    char temp_filename[] = "temp_merged.tof";

    // Open temporary file
    TOF_open(&temp_f, temp_filename, 'N');

    // Calculate total active records
    long total_records = 0;
    long nblocks = getHeader(f, "nBlock");

    // First, count total active records
    printf("\nCounting active records...\n");

    // Count in TOF
    for (long block_num = 1; block_num <= nblocks; block_num++) {
        TOF_readBlock(f, block_num, &buf);
        for (int pos = 0; pos < buf.nb; pos++) {
                total_records++;

        }
    }

    // Count in overflow
    for (long block_num = 1; block_num <= nblocks; block_num++) {
        TOF_readBlock(f, block_num, &buf);
        long of_block = buf.link;

        while (of_block != -1) {
            LnOF_readBlock(l, of_block, &bufl);
            for (int pos = 0; pos < bufl.nb; pos++) {
                    total_records++;
            }
            of_block = bufl.link;
        }
    }

    printf("Total active records found: %ld\n", total_records);

    // Allocate memory for all records
    t_rec *all_records = malloc(total_records * sizeof(t_rec));
    if (!all_records) {
        printf("Error: Memory allocation failed for %ld records!\n", total_records);
        TOF_close(temp_f);
        remove(temp_filename);
        return;
    }

    int record_count = 0;

    // 1. Collect records from TOF blocks
    printf("Collecting records from TOF...\n");
    for (long block_num = 1; block_num <= nblocks; block_num++) {
        TOF_readBlock(f, block_num, &buf);

        for (int pos = 0; pos < buf.nb; pos++) {
                all_records[record_count] = buf.tab[pos];
                record_count++;

        }
    }
    printf("Collected %d records from TOF\n", record_count);

    // 2. Collect records from overflow blocks
    printf("Collecting records from overflow...\n");
    int overflow_count = 0;
    for (long block_num = 1; block_num <= nblocks; block_num++) {
        TOF_readBlock(f, block_num, &buf);
        long of_block = buf.link;

        while (of_block != -1) {
            LnOF_readBlock(l, of_block, &bufl);

            for (int pos = 0; pos < bufl.nb; pos++) {

                    all_records[record_count] = bufl.tab[pos];
                    record_count++;
                    overflow_count++;

            }

            of_block = bufl.link;
        }
    }
    printf("Collected %d records from overflow\n", overflow_count);

    // 3. Sort all records
    printf("Sorting %d records...\n", record_count);
    for (int i = 0; i < record_count - 1; i++) {
        for (int j = 0; j < record_count - i - 1; j++) {
            if (all_records[j].from_id > all_records[j + 1].from_id ||
                (all_records[j].from_id == all_records[j + 1].from_id &&
                 all_records[j].to_id > all_records[j + 1].to_id) ||
                (all_records[j].from_id == all_records[j + 1].from_id &&
                 all_records[j].to_id == all_records[j + 1].to_id &&
                 all_records[j].timestamp > all_records[j + 1].timestamp)) {

                t_rec temp = all_records[j];
                all_records[j] = all_records[j + 1];
                all_records[j + 1] = temp;
            }
        }
    }

    // 4. Bulk load sorted records into temporary file with 75% loading factor
    printf("Loading sorted records into new file...\n");

    int j = 0;
    long i = 1;
    t_block merge_buf;
    merge_buf.nb = 0;
    merge_buf.link = -1;

    for (int k = 0; k < MAXTAB; k++) {
        merge_buf.del[k] = ' ';
    }

    int max_records_per_block = (int)(MAXTAB * 0.75);

    for (int k = 0; k < record_count; k++) {
        if (j < max_records_per_block) {
            merge_buf.tab[j] = all_records[k];
            merge_buf.del[j] = ' ';
            j++;
        } else {
            merge_buf.nb = j;
            TOF_writeBlock(temp_f, i, &merge_buf);
            i++;

            // Start new block
            merge_buf.tab[0] = all_records[k];
            merge_buf.del[0] = ' ';
            for (int m = 1; m < MAXTAB; m++) {
                merge_buf.del[m] = ' ';
            }
            j = 1;
        }
    }

    // Write last block if it has records
    if (j > 0) {
        merge_buf.nb = j;
        TOF_writeBlock(temp_f, i, &merge_buf);
    } else {
        i--;  // Adjust block count if last block was empty
    }

    // Update headers of temporary file
    setHeader(temp_f, "nBlock", i);
    setHeader(temp_f, "nIns", record_count);
    setHeader(temp_f, "nDel", 0);

    // Save temporary file headers to disk
    TOF_close(temp_f);

    // 5. Reset overflow file (not create new one)
    printf("Resetting overflow file...\n");

    // Read current overflow file to close it properly
    LnOF_close(l);

    // Reopen overflow file in 'N' mode to reset it
    LnOF_open(&l, "insertion", 'N');

    printf("Overflow file reset to empty state\n");

    // 6. Replace main file with temporary file
    printf("Replacing main file with merged file...\n");

    // Close current main file
    TOF_close(f);

    // Delete old main file
    char main_filename[100];
    strcpy(main_filename,file_name);

    // Copy temporary file to main file
    FILE *source = fopen(temp_filename, "rb");
    FILE *dest = fopen(main_filename, "wb");

    if (source && dest) {
        char buffer[1024];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
            fwrite(buffer, 1, bytes, dest);
        }
        fclose(source);
        fclose(dest);

        printf("Merged file saved as: %s\n", main_filename);

        // Delete temporary file
        remove(temp_filename);
    } else {
        printf("Error copying file! Using temporary file as main.\n");
        strcpy(main_filename, temp_filename);
    }

    // 7. Reopen the new merged file
    TOF_open(&f, main_filename, 'E');

    free(all_records);

    printf("\n=== REORGANIZATION COMPLETE ===\n");
    printf("1. Main file merged and sorted: %s\n", main_filename);
    printf("2. Overflow file reset to empty\n");
    printf("3. Total records: %d\n", record_count);
    printf("4. Number of blocks: %ld\n", getHeader(f, "nBlock"));
    printf("5. Loading factor: 75%% (design)\n");

    // Ask if user wants to display the new file
    printf("\nDo you want to display the new sorted file? (1=Yes, 0=No): ");
    scanf("%d", &confirm);

    if (confirm == 1) {
        range(false);
    }
}

int main()
{
   int choice;
	long i,j,k=0,rec_remained=1531;
	bool found,inside_overflow,only_one;
   char name[100], mode[20];
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

   // Menu principal  ...
  do {
    printf("\n--------- M E N U ( filename = '%s' ) ---------\n", name );
    printf("1) Display TOF header contents\n");
    printf("2) Display one TOF block contents\n");
    printf("3) Display LnOF Header\n");
    printf("4) Display overflow zones\n");
    printf("5) Display all the file\n");
    printf("6) Insert new records\n");
    printf("7) Search rating between two students\n");
    printf("8) Search for a record\n");
    printf("9) Update an existing rating\n");
    printf("10) List all friends of a given student\n");
    printf("11) Reorganize files (merge TOF and LnOF)\n");
    printf("12) Exit\n");
    scanf(" %d", &choice);
    printf("\n ------------------------------------------\n\n");

	switch(choice) {
	   case 1:
		   info_TOF();
			break;
		case 2: {
		    only_one=true;
		    range(only_one);
		    break;
		    }
		     case 3: info_LnOF();
         break;
         case 4: display_all_overflows();
			break;
			case 5: {
			    info_TOF();
		    only_one=false;
		    range(only_one);
		    break;
		    }
		case 6: {
				t_block buf = {0};  // ZERO ALL FIELDS
				t_block bufl = {0};
				insertion(insertion_table,&k,&rec_remained);
				break;
			}
			case 7: search_rating();
			break;
			case 8: {
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
			 case 9:
            update_rating();
            break;

        case 10:
            list_friends_of_student();
            break;

       case 11: reorganize_files(name);
       break;
        case 12:
             break;

        default: printf("Invalid choice! Please try again.\n");
    }
} while (choice != 12);
   // Closing the file (and save headers)
   TOF_close(f);
	LnOF_close(l);

   return 0;

} // main







