/*
 * Autor: Samuel Hetteš
 * Predmet: Dátové šturktúry a algoritmy
 * Zadanie: Správca pamäti
 * Metóda: Explicit List
 * Prostredie: CLion
 * Dátum: 6. 3. 2021
*/



/* HEADERS */

#include <stdio.h>
#include <stdlib.h>    //rand
#include <string.h>    //memset
#include <time.h>    //rand seed



/* VARIABLES */

//globálny ukazovateľ na začiatok pamäte
void *global_ptr;

//reprezentácia jednotlivých bytov
#define FREE_BYTE 0
#define FULL_BYTE 2
#define HANGING_BYTE -2

//konštantné veľkosti
#define MAIN_HEADER_SIZE 12
#define FOOTER_SIZE 4
#define FULL_BLOCK_HEADER_SIZE 4
#define FULL_BLOCK_DESIGN_SIZE 8
#define FREE_BLOCK_DESIGN_SIZE 24
#define NEXT_PREVIOUS_PTR_SIZE 16



/* DEFINED TYPES */

//vypnutie automatického zarovnávania veľkosti štruktúr
#pragma pack(1)

//hlavička alokovaného bloku = 4B
typedef struct full_block_header
{
    unsigned int size;
} FULL_BLOCK_HEADER;

//hlavička voľného bloku = 20B
typedef struct header
{
    unsigned int size;
    struct header *next;
    struct header *previous;
} HEADER;

//pätička voľného/alokovaného bloku = 4B
typedef struct footer
{
    unsigned int size;
} FOOTER;

//hlavná(globálna) hlavička = 12B
typedef struct main_header
{
    unsigned int size;
    HEADER *start;
} MAIN_HEADER;



/* MACROS */

//overovacie makrá
#define IS_HANGING_BYTE_AFTER(ptr) (*((unsigned int *)ptr) == 4278124286)
#define IS_HANGING_BYTE_BEFORE(ptr) (*((unsigned int *)ptr) == 4278124286)
#define IS_MULTIPLE_OF_4(size) (!(size % 4))
#define IS_ODD(size) (size & 1)

//overovacie makrá pri uvoľnovaní bloku
#define BOTH_ALLOC(previous_footer, next_header) ((previous_footer->size & 1) && (next_header->size & 1))
#define PREVIOUS_FREE(previous_footer, next_header) ((!(previous_footer->size & 1)) && (next_header->size & 1))
#define NEXT_FREE(previous_footer, next_header) ((previous_footer->size & 1) && (!(next_header->size & 1)))
#define BOTH_FREE(previous_footer, next_header) ((!(previous_footer->size & 1)) && (!(next_header->size & 1)))

//zaokrúhlenie čísla na párne
#define ROUND_TO_MULTIPLE_OF_4(size) (IS_MULTIPLE_OF_4(size) ? size : (((size + 3) >> 2) << 2))

//maskovanie, odmaskovanie
#define MASK(size) (size | 1)
#define MASK_OUT(size) (size & (~1))

//spájanie 2 a 3 blokov, získanie veľkosti úložiska bloku a odčítanie veľkostí pointrov *next a *previous
#define MERGE_2_SIZE(free_size, freed_size) (free_size + freed_size + FULL_BLOCK_DESIGN_SIZE)
#define MERGE_3_SIZE(previous_free_size, freed_size, next_free_size) (previous_free_size + freed_size + next_free_size + 2 * FULL_BLOCK_DESIGN_SIZE)
#define GET_PAYLOAD_SIZE(size) (size - FULL_BLOCK_DESIGN_SIZE)
#define SUB_POINTERS_SIZE(size) (size - NEXT_PREVIOUS_PTR_SIZE)

//získanie konkrétnej adresy hlavičiek, pätičiek, začiatku a konca pamäte, začiatku úložiska bloku
#define GET_PREVIOUS_HEADER(header, previous_footer) ((HEADER *)((char *)header - FULL_BLOCK_DESIGN_SIZE - previous_footer->size))
#define GET_NEXT_HEADER(footer, offset) ((HEADER *)((char *)footer + FOOTER_SIZE + offset))
#define GET_FULL_BLOCK_HEADER(ptr) ((FULL_BLOCK_HEADER *)((char *)ptr - FULL_BLOCK_HEADER_SIZE))

#define GET_FOOTER(header) ((FOOTER *)((char *)header + FULL_BLOCK_HEADER_SIZE + header->size))
#define GET_PREVIOUS_FOOTER(header, offset) ((FOOTER *)((char *)header - FOOTER_SIZE - offset))
#define GET_FULL_BLOCK_FOOTER(header) ((FOOTER *)((char *)header + MASK_OUT(header->size) + FULL_BLOCK_HEADER_SIZE))

#define GET_START(global_ptr) ((void *)((char *)global_ptr + MAIN_HEADER_SIZE))
#define GET_END(global_ptr) ((void *)((char *)global_ptr + MAIN_HEADER_SIZE + ((MAIN_HEADER *)global_ptr)->size))

#define TO_PAYLOAD(header) (header + 1)



/* MEMORY FUNCTIONS */

int memory_check(void *ptr)
{
    void *end = GET_END(global_ptr);
    void *start = GET_START(global_ptr);

    FULL_BLOCK_HEADER *header = GET_FULL_BLOCK_HEADER(ptr);
    FOOTER *footer = GET_FULL_BLOCK_FOOTER(header);

    /*
    Ukazovateľ je neplatný ak: 
    •	ukazuje za koniec alebo pred začiatok pamäte
    •	pätička ukazuje za koniec pamäte alebo hlavička pred začiatok pamäte
    •	veľkosť v hlavičke nie je nepárne číslo (alokovaný blok) alebo násobok 4
    •	sa veľkosť v hlavičke a pätičke nerovnajú
    */

    if ((ptr >= end) || (ptr < start) || (footer >= (FOOTER *)end) || (header < (FULL_BLOCK_HEADER *)start))
        return 0;
    else if ((!IS_ODD(header->size)) || (header->size != footer->size) || !IS_MULTIPLE_OF_4(MASK_OUT(header->size)))
        return 0;

    return 1;    //ak je pointer platný vrátim 1
}


void memory_init(void *ptr, unsigned int size)
{
    global_ptr = ptr;

    MAIN_HEADER *mainHeader = (MAIN_HEADER *) global_ptr;

    //nastavenie veľkosti hlavnej hlavičky, ak by celková veľkosť pamäte nebola násobkom 4, zvyšné koncové byty ostanú nevyužívané
    if (IS_MULTIPLE_OF_4(size))
        mainHeader->size = size - MAIN_HEADER_SIZE;
    else
        mainHeader->size = size - MAIN_HEADER_SIZE - (size % 4);

    HEADER *firstHeader = (HEADER *)TO_PAYLOAD(mainHeader);    //nastavenie hlavičky prvého voľného bloku
    firstHeader->size = GET_PAYLOAD_SIZE(mainHeader->size);
    firstHeader->next = NULL;
    firstHeader->previous = NULL;

    mainHeader->start = firstHeader;    //nastavenie štartovacej hlavičky

    FOOTER *firstFooter = GET_FOOTER(firstHeader);    //nastavenie pätičky
    firstFooter->size = firstHeader->size;

    memset(TO_PAYLOAD(firstHeader), FREE_BYTE, SUB_POINTERS_SIZE(firstHeader->size));    //označenie bytov
}


void *memory_alloc(unsigned int size)
{
    if (size <= 0)
        return NULL;

    HEADER *header = ((MAIN_HEADER *)global_ptr)->start;

    size = ROUND_TO_MULTIPLE_OF_4(size);    //zaokrúhlenie veľkosti na násobok 4

    while ((header) && (header->size < size))    //nájdenie vhodného bloku
        header = header->next;

    if (!header)    //ak blok nebol nájdený vráť NULL
        return NULL;

    HEADER *headerBefore = header->previous;
    HEADER *headerAfter = header->next;

    //spojenie predchádzajúcej a nasledujúcej hlavičky
    if (headerBefore)
        headerBefore->next = headerAfter;
    if (headerAfter)
        headerAfter->previous = headerBefore;

    unsigned int new_block_size = header->size - size;    //veľkost nového bloku

    if (!new_block_size)    //nová veľkosť je rovnaka ako veľkosť bloku
    {
        memset(TO_PAYLOAD((FULL_BLOCK_HEADER *)header), FULL_BYTE, size);

        if (((MAIN_HEADER *)global_ptr)->start == header)    //over či začiatok neukazuje na alokovaný blok
            ((MAIN_HEADER *)global_ptr)->start = headerAfter;
    }

    else if (new_block_size < FREE_BLOCK_DESIGN_SIZE)    //nová veľkosť je menšia ako veľkosť potrebná na uchovanie ukazovateľ
    {
        memset(TO_PAYLOAD((FULL_BLOCK_HEADER *)header), FULL_BYTE, size);
        memset((char *)header + FULL_BLOCK_DESIGN_SIZE + size, HANGING_BYTE, new_block_size);    //označ zvyšné byty ako visiace

        if (((MAIN_HEADER *)global_ptr)->start == header)    //over či začiatok neukazuje na alokovaný blok
            ((MAIN_HEADER *)global_ptr)->start = headerAfter;
    }

    else    //inak rozdeľujeme blok, nový blok pridáme na začiatok
    {
        memset(TO_PAYLOAD((FULL_BLOCK_HEADER *)header), FULL_BYTE, size);

        HEADER *freeHeader = (HEADER *)((char *)header + FULL_BLOCK_DESIGN_SIZE + size);    //nastav hlavičku voľného bloku
        freeHeader->size = GET_PAYLOAD_SIZE(new_block_size);
        freeHeader->previous = NULL;

        if (((MAIN_HEADER *)global_ptr)->start == header)    //over či začiatok neukazuje na alok. blok
            freeHeader->next = headerAfter;
        else
            freeHeader->next = ((MAIN_HEADER *)global_ptr)->start;    //nasledujúci blok bude štartovací blok

        if (freeHeader->next)    //ak existuje prepoj ho s pridaným blokom
            (freeHeader->next)->previous = freeHeader;

        FOOTER *freeFooter = GET_FOOTER(freeHeader);    //nastav pätičku nového bloku
        freeFooter->size = freeHeader->size;

        ((MAIN_HEADER *)global_ptr)->start = freeHeader;    //nastav začiatok na tento blok
    }

    header->size = MASK(size);    //zamaskuj hlavičku alokovaného bloku

    FOOTER *footer = GET_FULL_BLOCK_FOOTER(header);    //nastav pätičku alokovaného bloku
    footer->size = header->size;

    return (TO_PAYLOAD((FULL_BLOCK_HEADER *)header));    //vráť pointer na úložisko alokovaného bloku
}


int memory_free(void *valid_ptr)
{
    HEADER *header = (HEADER *)GET_FULL_BLOCK_HEADER(valid_ptr);
    FOOTER *footer = GET_FULL_BLOCK_FOOTER(header);

    unsigned int hanging_bytes_after = 0;    //visiace byty pred hlavičkou a visiace byty za pätičkou uvoľnovaného bloku
    unsigned int hanging_bytes_before = 0;

    char *end = (char *)GET_END(global_ptr);
    char *start = (char *)GET_START(global_ptr);

    char *ptr = (char *)(footer + 1);    //pointer za pätičku

    while (ptr < end && IS_HANGING_BYTE_AFTER(ptr))    //prečítanie visiacich bytov pred hlavičkou
    {
        hanging_bytes_after += 4;
        ptr += 4;
    }

    ptr = (char *)header - 4;    //pointer pred hlavičku

    while (ptr >= start && IS_HANGING_BYTE_BEFORE(ptr))    //prečítanie visiacich bytov za pätičkou
    {
        hanging_bytes_before += 4;
        ptr -= 4;
    }

    FOOTER *previous_footer = GET_PREVIOUS_FOOTER(header, hanging_bytes_before);
    HEADER *next_header = GET_NEXT_HEADER(footer, hanging_bytes_after);

    if (previous_footer < (FOOTER *)start)    //ak siaha pred našu pamäť, nastav ju na aktuálnu pätičku(ako keby alok. blok)
        previous_footer = footer;

    if (next_header >= (HEADER *)end)    //ak hlavička siaha za pamäť, nastav ju na aktuálnu hlavičku(ako keby alok. blok)
        next_header = header;

    //predchádzajúci a nasledujúci blok je alokovaný, pridáme uvoľňovaný blok na začiatok zoznamu
    if (BOTH_ALLOC(previous_footer, next_header))
    {
        unsigned int size = MASK_OUT(header->size) + hanging_bytes_before + hanging_bytes_after;

        header = (HEADER *)((char *)header - hanging_bytes_before);    //posunieme hlavičku a pätičku o možné visiace byty
        footer = (FOOTER *)((char *)footer + hanging_bytes_after);

        //ak by uvoľňovaná veľkosť bola menšia ako veľkosť pointrov hlavičky, nastav byty ako vysiace a vráť 0
        if (size < NEXT_PREVIOUS_PTR_SIZE)
        {
            memset(header, HANGING_BYTE, size + FULL_BLOCK_DESIGN_SIZE);

            return 0;
        }

        header->size = size;    //inak nastav hlavičku a pätičku uvoľňovaného bloku
        header->previous = NULL;
        header->next = ((MAIN_HEADER *)global_ptr)->start;    //nasledujúci blok bude blok, na ktorý ukazuje začiatok

        footer->size = header->size;

        if (header->next)    //prepoj hlavičku s nasledujúcim blokom ak existuje
            (header->next)->previous = header;

        ((MAIN_HEADER *)global_ptr)->start = header;    //nastav začiatok na uvoľnený blok

        memset(TO_PAYLOAD(header), FREE_BYTE, SUB_POINTERS_SIZE(header->size));

        return 0;
    }

    //predchádzajúci blok je voľný a nasledujúci alokovaný, spojíme predchádzajúci blok s uvoľňovaným
    if (PREVIOUS_FREE(previous_footer, next_header))
    {
        header = GET_PREVIOUS_HEADER(header, previous_footer);    //získaj hlavičku predchádzajúceho bloku(hlavička spojeného bloku) a nastav ju
        header->size = MERGE_2_SIZE(previous_footer->size, MASK_OUT(footer->size)) + hanging_bytes_after;

        footer = GET_FOOTER(header);    //nastav pätičku
        footer->size = header->size;

        memset(TO_PAYLOAD(header), FREE_BYTE, SUB_POINTERS_SIZE(header->size));

        return 0;
    }

    //nasledujúci blok je voľný a predchádzajúci alokovaný, spoj nasledujúci s uvoľňovaným
    if (NEXT_FREE(previous_footer, next_header))
    {
        header = (HEADER *)((char *)header - hanging_bytes_before);    //nastav hlavičku pre spojený blok
        header->size = MERGE_2_SIZE(next_header->size, MASK_OUT(footer->size)) + hanging_bytes_before;
        header->previous = next_header->previous;

        if (header->previous)    //prepoj predchádzajúci blok s novým blokom
            (header->previous)->next = header;

        header->next = next_header->next;

        if (header->next)    //prepoj nasledujúci blok s novým blokom
            (header->next)->previous = header;

        if (((MAIN_HEADER *)global_ptr)->start == next_header)    //over či začiatok neukazoval na hlavičku nasledujúceho bloku, kt. sa spojil s uvoľňovaným
            ((MAIN_HEADER *)global_ptr)->start = header;

        footer = GET_FOOTER(header);    //získaj pätičku a nastav ju
        footer->size = header->size;

        memset(TO_PAYLOAD(header), FREE_BYTE, SUB_POINTERS_SIZE(header->size));

        return 0;
    }

    //predchádzajúci a nasledujúci blok je voľný, spoj tieto bloky s uvoľňovaným
    if (BOTH_FREE(previous_footer, next_header))
    {
        header = GET_PREVIOUS_HEADER(header, previous_footer);    //hlavička predchádzajúceho bloku bude predstavovať hlavičku nového bloku, získaj ju a nastav
        header->size = MERGE_3_SIZE(previous_footer->size, MASK_OUT(footer->size), next_header->size);

        //ak predchádzajúci ukazuje na nasledujúci blok, prepoj next nasledujúceho s novým blokom
        if (header->next == next_header)
        {
            header->next = next_header->next;

            if (header->next)
                (header->next)->previous = header;
        }
            //ak next nasledujúceho je predchádzajúci blok, prepoj previous nasledujúceho s novým blokom
        else if (next_header->next == header)
        {
            header->previous = next_header->previous;

            if (header->previous)
                (header->previous)->next = header;

            if (((MAIN_HEADER *)global_ptr)->start == next_header)    //over či začiatok neukazoval na nasledujúci blok
                ((MAIN_HEADER *)global_ptr)->start = header;
        }
        else
        {
            //ak previous ukazuje na nejaký blok, musíme nasledujúci blok odpojiť zo zoznamu
            if (next_header->previous)
            {
                HEADER *tempHeader = ((MAIN_HEADER *)global_ptr)->start;

                while (tempHeader->next && tempHeader->next != next_header)    //nájdi blok, ktorý ukazuje na tento blok
                    tempHeader = tempHeader->next;

                tempHeader->next = next_header->next;

                if (next_header->next)
                    (next_header->next)->previous = tempHeader;
            }
                //ak nemá predchodcu, tak tento blok je začiatočný, posunieme začiatok na nasledovníka tohto bloku
            else
            {
                ((MAIN_HEADER *)global_ptr)->start = next_header->next;
                if (next_header->next)
                    (next_header->next)->previous = NULL;
            }

        }

        footer = GET_FOOTER(header);    //nastav pätičku
        footer->size = header->size;

        memset(TO_PAYLOAD(header), FREE_BYTE, SUB_POINTERS_SIZE(header->size));

        return 0;
    }

    return 1;    //ak sa nepodarilo uvoľniť blok vráť 1
}



/* TESTER */

void memory_tester(unsigned int min_block_size, unsigned int max_block_size, unsigned int min_mem_size, unsigned int max_mem_size, int tests, int same_sizes_only, int alloc_only)
{
    unsigned int random_mem_size;
    random_mem_size = (rand() % (max_mem_size - min_mem_size + 1)) + min_mem_size;    //náhodná veľkosť pamäte

    char region[random_mem_size];    //región pamäte

    memory_init(region, random_mem_size);    //prv zavolaj memory init

    int index = 0;
    int random_index;
    unsigned int random_size;
    unsigned int allocated_bytes = 0;
    unsigned int requested_bytes = 0;
    unsigned int allocated_blocks = 0;
    unsigned int requested_blocks = 0;
    long int currently_free_bytes = (long int)random_mem_size;


    //vytvoríme si polia dostatočnej veľkosti, kde budeme ukladať vrátené ukazovatele a alokované veľkosti
    unsigned int max_blocks = (random_mem_size - MAIN_HEADER_SIZE) / (min_block_size + FULL_BLOCK_DESIGN_SIZE);
    char *pointers[max_blocks];
    unsigned int sizes[max_blocks];

    for (unsigned int i = 0; i < max_blocks; i++)
        pointers[i] = NULL;
        

    random_size = (rand() % (max_block_size - min_block_size + 1)) + min_block_size;    //náhodná veľkosť bloku

    if (alloc_only)    //iba alokácia blokov
    {
        tests = 0;
        while (currently_free_bytes >= 0)    //pokým je veľkosť pamäte dostatočná, alokuj bloky
        {
            if (!same_sizes_only)    //ak nie je aktivovaná rovnaká veľkosť blokov, získaj náhodnú veľkosť
                random_size = (rand() % (max_block_size - min_block_size + 1)) + min_block_size;

            if ((currently_free_bytes - (long int)random_size) >= 0)    //ak nám pamäť stačí na uloženie bloku
            {
                pointers[index] = memory_alloc(random_size);    //vrátený pointer ulož do poľa
                sizes[index] = random_size;    //veľkosť, kt. bola alokovaná ulož taktiež do poľa


                requested_bytes += random_size;    //zvýš požadované byty a bloky, odpočítaj od voľných bytov alokovanú veľkosť
                requested_blocks++;
                currently_free_bytes -= (long int) random_size;

                if (pointers[index])    //ak nebol vrátený NULL pointer zvýš alokované byty, bloky a index
                {
                    index++;
                    allocated_bytes += random_size;
                    allocated_blocks++;
                }
            }
            else    //ak už nie je voľné miesto pre ďaľší blok vyskoč z cyklu
                break;
        }
    }

    for (int i = 0; i < tests; i++)    //cyklus, ktorý bude volať náhodne funkcie na základe počtu testov
    {
        switch(rand() % 2)    //náhodne vyvolá funkciu buď memory_alloc alebo memory_free
        {
            case 0:   //alokácia funguje na rovnakom princípe ako vyššie
                if (!same_sizes_only)
                    random_size = (rand() % (max_block_size - min_block_size + 1)) + min_block_size;

                pointers[index] = memory_alloc(random_size);
                sizes[index] = random_size;

                if ((currently_free_bytes - (long int)random_size) >= 0)
                {
                    requested_bytes += random_size;
                    requested_blocks++;
                    currently_free_bytes -= (long int)random_size;

                    if (pointers[index])
                    {
                        index++;
                        allocated_bytes += random_size;
                        allocated_blocks++;
                    }
                }
                break;

            case 1:    //uvoľňovanie bloku
                if (index != 0)    //ak už bol uložený nejaký pointer
                {
                    random_index = rand() % index;    //vyber náhodne jeden z nich a uvoľni ho

                    memory_free(pointers[random_index]);
                    pointers[random_index] = NULL;
                    index--;

                    currently_free_bytes += (long int)sizes[random_index];    //pripočítaj uvoľnenú veľkosť k aktuálne voľným bytom

                    for (int j = random_index; pointers[j+1]; j++)    //popresúvaj pole pointrov a veľkostí
                    {
                        pointers[j] = pointers[j+1];
                        sizes[j] = sizes[j+1];
                    }

                }
                break;
        }
    }

    char *ptr = (char *) global_ptr;    //pointer prv ukazuje na začiatok
    char *end = ptr + random_mem_size;    //koniec pamäte
    int valid = 0;    //pomocná premenná

    //memory check všetkých adries pamäte do konca pamäte
    while (ptr <= end)
    {
        for (int i = 0; i < index; i++)    //cyklus, ktorý prejde ukazovateľmi, ktoré boli vrátené funkciou memory alloc
        {
            if (ptr == pointers[i])    //ak nájde zhodu nastaví valid na 1
                valid = 1;
        }
        if (!memory_check(ptr) && valid)    //zavoláme memory check, ak vráti 0 a valid bol 1, došlo k nesprávnemu overeniu ukazovateľa
            printf("Wrong memory check at adress: %p\n", (void *)ptr);
        valid = 0;
        ptr++;
    }

    //na záver percentuálne vyjadríme počet alokovaných bytov a blokov, vypíšeme správu o teste a jeho výsledkoch
    float blocks_result = (float)allocated_blocks / (float)requested_blocks * 100;
    float bytes_result = (float)allocated_bytes / (float)requested_bytes * 100;

    if (same_sizes_only && alloc_only)
        printf("*Allocating blocks of size: %u*\n", random_size);
    else if (!same_sizes_only && alloc_only)
        printf("*Allocating blocks of random size*\n");
    else if (same_sizes_only && !alloc_only)
        printf("*Allocating and freeing blocks of size: %u*\n", random_size);
    else
        printf("*Allocating and freeing blocks of random size*\n");

    printf("Memory size: %u\n", random_mem_size);
    printf("Allocated blocks: %.2f%%\n", blocks_result);
    printf("Allocated bytes: %.2f%%\n\n", bytes_result);
}



/* MAIN */

int main(void)
{
    srand(time(0));

    unsigned int min_block_size;
    unsigned int max_block_size;
    unsigned int min_mem_size;
    unsigned int max_mem_size;
    int tests;    //počet testov pri náhodnej alokácii a uvoľňovaní
    int same_sizes_only;    //možnosť nastaviť na 1 ak chceme, aby bloky boli rovnakej veľkosti
    int alloc_only;    //možnosť nastaviť na 1 ak chceme iba alokovať bloky a nie uvoľňovať

    //alokovanie blokov, prvý test alokuje bloky rovnakej veľkosti, premenná tests musí byť 0, keďže budeme alokovať dokým nám bude stačiť pamäť
    memory_tester(min_block_size = 8, max_block_size = 24, min_mem_size = 50, max_mem_size = 200, tests = 0, same_sizes_only = 1, alloc_only = 1);
    memory_tester(min_block_size = 8, max_block_size = 24, min_mem_size = 50, max_mem_size = 200, tests = 0, same_sizes_only = 0, alloc_only = 1);
    memory_tester(min_block_size = 500, max_block_size = 5000, min_mem_size = 10000, max_mem_size = 100000, tests = 0, same_sizes_only = 0, alloc_only = 1);
    memory_tester(min_block_size = 8, max_block_size = 50000, min_mem_size = 100000, max_mem_size = 1000000, tests = 0, same_sizes_only = 0, alloc_only = 1);


    //alokovanie a uvoľňovanie blokov, prvý test alokuje bloky rovnakej veľkosti
    memory_tester(min_block_size = 8, max_block_size = 24, min_mem_size = 50, max_mem_size = 200, tests = 1000, same_sizes_only = 1, alloc_only = 0);
    memory_tester(min_block_size = 8, max_block_size = 24, min_mem_size = 50, max_mem_size = 200, tests = 1000, same_sizes_only = 0, alloc_only = 0);
    memory_tester(min_block_size = 500, max_block_size = 5000, min_mem_size = 10000, max_mem_size = 100000, tests = 10000, same_sizes_only = 0, alloc_only = 0);
    memory_tester(min_block_size = 8, max_block_size = 50000, min_mem_size = 100000, max_mem_size = 1000000, tests = 100000, same_sizes_only = 0, alloc_only = 0);

    return 0;
}


/* VLASTNÉ TESTY */

/*
Test č. 1 - memory init

unsigned int size = 100;
char region[size];
memory_init(region, size);


Test č. 2 - memory alloc

unsigned int size = 100;
char region[size];
memory_init(region, size);
memory_alloc(80);


Test č. 3 - memory alloc

unsigned int size = 100;
char region[size];
memory_init(region, size);
memory_alloc(60);


Test č. 4 - memory alloc

unsigned int size = 100;
char region[size];
memory_init(region, size);
memory_alloc(40);


Test č. 5 - memory free

unsigned int size = 100;
char region[size];
memory_init(region, size);
memory_alloc(12);
void *ptr = memory_alloc(32);
memory_alloc(12);
memory_free(ptr);


Test č. 6 - memory free

unsigned int size = 100;
char region[size];
memory_init(region, size);
memory_alloc(12);
void *ptr = memory_alloc(12);
memory_alloc(12);
memory_free(ptr);


Test č. 7 - memory free

unsigned int size = 100;
char region[size];
memory_init(region, size);
void *ptr1 = memory_alloc(12);
void *ptr2 = memory_alloc(12);
void *ptr3 = memory_alloc(12);
memory_alloc(12);
memory_free(ptr1);
memory_free(ptr3);
memory_free(ptr2);


Test č. 8 - memory free

unsigned int size = 100;
char region[size];
memory_init(region, size);
void *ptr1 = memory_alloc(16);
void *ptr2 = memory_alloc(12);
void *ptr3 = memory_alloc(12);
memory_alloc(12);
memory_free(ptr1);
memory_free(ptr3);
memory_free(ptr2);


Test č. 9 - memory free

unsigned int size = 100;
char region[size];
memory_init(region, size);
memory_alloc(16);
void *ptr2 = memory_alloc(12);
void *ptr3 = memory_alloc(12);
memory_free(ptr2);
memory_free(ptr3);


Test č. 10 - memory free

unsigned int size = 100;
char region[size];
memory_init(region, size);
void *ptr1 = memory_alloc(16);
void *ptr2 = memory_alloc(12);
void *ptr3 = memory_alloc(36);
memory_free(ptr1);
memory_free(ptr3);
memory_free(ptr2);

*/