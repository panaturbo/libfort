#include "stdlib.h"
#include "stdarg.h"
#include "stdio.h"
#include "fort.h"
#include "assert.h"
#include "string.h"
#include "wchar.h"

#define FORT_COL_SEPARATOR '|'
#define FORT_UNUSED  __attribute__((unused))

#define F_CALLOC calloc
#define F_MALLOC malloc
#define F_REALLOC realloc
#define F_FREE free
#define F_STRDUP fort_strdup

#define F_CREATE(type) ((type *)F_CALLOC(sizeof(type), 1))

#define MAX(a,b) ((a) > (b) ? (a) : b)
#define MIN(a,b) ((a) < (b) ? (a) : b)



#define STR_2_CAT_(arg1, arg2) \
    arg1##arg2
#define STR_2_CAT(arg1, arg2) \
    STR_2_CAT_(arg1, arg2)

#define UNIQUE_NAME_(prefix) \
    STR_2_CAT(prefix,__COUNTER__)
#define UNIQUE_NAME(prefix) \
    UNIQUE_NAME_(prefix)


/*****************************************************************************
 *               LOGGER
 *****************************************************************************/
#define SYS_LOG_ERROR(...)


/*****************************************************************************
 *               RETURN CODES
 * ***************************************************************************/
typedef int fort_status_t;
#define F_SUCCESS        0
#define F_MEMORY_ERROR   1
#define F_ERROR          2
#define IS_SUCCESS(arg) ((arg) == F_SUCCESS)
#define IS_ERROR(arg) (!IS_SUCCESS(arg))




/*****************************************************************************
 *               STRING BUFFER
 * ***************************************************************************/
struct string_buffer;
typedef struct string_buffer string_buffer_t;
struct string_buffer
{
    char *str;
    size_t str_sz;
};

static string_buffer_t* create_string_buffer(size_t sz);
static void destroy_string_buffer(string_buffer_t *buffer);
static fort_status_t realloc_string_buffer_without_copy(string_buffer_t *buffer);
static fort_status_t fill_buffer_from_string(string_buffer_t *buffer, const char *str);

/*****************************************************************************
 *               VECTOR
 * ***************************************************************************/

struct vector;
typedef struct vector vector_t;

#define INVALID_VEC_INDEX ((size_t) -1)

static vector_t* create_vector(size_t item_size, size_t capacity);
static void destroy_vector(vector_t*);

static size_t vector_size(const vector_t*);
static size_t vector_capacity(const vector_t*);
static size_t vector_index_of(const vector_t*, const void *item);

static int vector_push(vector_t*, const void *item);
static int vector_erase(vector_t*, size_t index);
static void vector_clear(vector_t*);
static void* vector_at(vector_t*, size_t index);


/*****************************************************************************
 *               CELL
 * ***************************************************************************/
struct cell_options
{
    int padding_top;
    int padding_bottom;
    int padding_left;
    int padding_right;
};
typedef struct cell_options cell_options_t;

void init_cell_options(cell_options_t *options)
{
    assert(options);
    options->padding_top = 1;
    options->padding_bottom = 1;
    options->padding_left = 1;
    options->padding_right = 1;
}

struct fort_cell;
typedef struct fort_cell fort_cell_t;
struct fort_cell
{
    string_buffer_t *str_buffer;
    cell_options_t options;
};

fort_cell_t * create_cell()
{
    fort_cell_t *cell = F_CALLOC(sizeof(fort_cell_t), 1);
    if (cell == NULL)
        return NULL;
    cell->str_buffer = create_string_buffer(1024);
    if (cell->str_buffer == NULL) {
        F_FREE(cell);
        return NULL;
    }
    init_cell_options(&(cell->options));
    return cell;
}

void destroy_cell(fort_cell_t *cell)
{
    if (cell == NULL)
        return;
    destroy_string_buffer(cell->str_buffer);
    F_FREE(cell);
}

int hint_width_cell(fort_cell_t *cell)
{
    assert(cell);
    int result = cell->options.padding_left + cell->options.padding_right;
    if (cell->str_buffer && cell->str_buffer->str) {
        result += strlen(cell->str_buffer->str);
    }
    return result;
}

int hint_height_cell(fort_cell_t *cell)
{
    assert(cell);
    int result = cell->options.padding_top + cell->options.padding_bottom;
    if (cell->str_buffer && cell->str_buffer->str) {
        result += 1;
    }
    return result;
}


/*
 * Returns number of lines in cell. If cell is empty or
 * contains empty string, then 0 is returned.
 */
int lines_number_cell(fort_cell_t *cell)
{
    assert(cell);
    if (cell->str_buffer == NULL || cell->str_buffer->str == NULL || cell->str_buffer->str[0] == '\0') {
        return 0;
    }

    int result = 0;
    char *pos = cell->str_buffer->str;
    while ((pos = strchr(pos, '\n')) != NULL) {
        result++;
        pos++;
    }
    return result + 1;
}

int hint_height_cell(fort_cell_t *cell)
{
    assert(cell);
    int result = cell->options.padding_top + cell->options.padding_bottom;
    if (cell->str_buffer) {
        result += lines_number_cell(cell);
    }
    return result;
}

/*****************************************************************************
 *               ROW
 * ***************************************************************************/
struct fort_row;
typedef struct fort_row fort_row_t;
struct fort_row
{
    vector_t *cells;
};


static fort_row_t * create_row()
{
    fort_row_t * row = F_CALLOC(sizeof(fort_row_t), 1);
    if (row == NULL)
        return NULL;
    row->cells = create_vector(sizeof(fort_cell_t*), 10);
    if (row->cells == NULL) {
        F_FREE(row);
        return NULL;
    }
    return row;
}

static void destroy_row(fort_row_t *row)
{
    if (row == NULL)
        return;

    if (row->cells) {
        size_t cells_n = vector_size(row->cells);
        for (size_t i = 0; i < cells_n; ++i) {
            fort_cell_t *cell = *(fort_cell_t **)vector_at(row->cells, i);
            destroy_cell(cell);
        }
        destroy_vector(row->cells);
    }

    F_FREE(row);
}

static fort_row_t * create_row_from_string(const char *str);
static fort_row_t* create_row_from_fmt_string(const char* FORT_RESTRICT fmt, va_list *va_args);

int columns_in_row(const fort_row_t *row)
{
    if (row == NULL || row->cells == NULL)
        return 0;

    return vector_size(row->cells);
}


/*****************************************************************************
 *               TABLE
 *****************************************************************************/
struct fort_table
{
    fort_row_t *header;
    vector_t *rows;
};


/*****************************************************************************
 *               LIBFORT helpers
 *****************************************************************************/
char *fort_strdup(const char* str)
{
    if (str == NULL)
        return NULL;

    size_t sz = strlen(str);
    char *str_copy = (char*)F_MALLOC(sz + 1);
    if (str_copy == NULL)
        return NULL;

    strcpy(str_copy, str);
    return str_copy;
}

size_t number_of_columns_in_format_string(const char *fmt)
{
    int separator_counter = 0;
    const char *pos = fmt;
    while (1) {
        pos = strchr(pos, FORT_COL_SEPARATOR);
        if (pos == NULL)
            break;

        separator_counter++;
        ++pos;
    }
    return separator_counter + 1;
}

/*****************************************************************************
 *               LIBFORT
 * ***************************************************************************/

FTABLE * ft_create_table(void)
{
    FTABLE *result = F_CALLOC(1, sizeof(FTABLE));
    if (result == NULL)
        return NULL;

    result->rows = create_vector(sizeof(fort_row_t*), 10);
    if (result->rows == NULL) {
        F_FREE(result);
        return NULL;
    }
    return result;
}


void ft_destroy_table(FTABLE *FORT_RESTRICT table)
{
    if (table == NULL)
        return;

    destroy_row(table->header);
    if (table->rows) {
        size_t row_n = vector_size(table->rows);
        for (size_t i = 0; i < row_n; ++i) {
            destroy_row(*(fort_row_t **)vector_at(table->rows, i));
        }
        destroy_vector(table->rows);
    }
    F_FREE(table);
}

//void forward_va_list(const char *fmt FORT_UNUSED, va_list *va)
//{
//    /* todo - do normal implementation */
//    va_arg(*va, int);
//    va_arg(*va, int);

//    while (*fmt) {
//        if (*fmt != '%') {
//            ++fmt;
//            continue;
//        }

//        char argument_type[2] = { '\0', '\0'};

//        check_format:
//        ++fmt;
//        /* todo: do normal checking for order of symbols. Don't think symbol checking can be in independent order. */
//        switch(*fmt) {
//            case '\0':
//                return;

//            /* flags */
//            case '-':
//            case '+':
//            case ' ':
//            case '#':
//            case '0':
//            case '1':
//            case '2':
//            case '3':
//            case '4':
//            case '5':
//            case '6':
//            case '7':
//            case '8':
//            case '9':
//            case '.':
//                goto check_format;


//            /* length modifiers */
//            case 'h':
//            case 'l':
//                if (argument_type[0] == '\0')
//                    argument_type[0] = *fmt;
//                else
//                    argument_type[1] = *fmt;
//                goto check_format;
//            case 'j':
//            case 'z':
//            case 't':
//            case 'L':
//                argument_type[1] = *fmt;
//                goto check_format;

//            /* specifiers */
//            case 'c':
//                if (argument_type[0] == 'l' && argument_type[0] == '\0')
//                    va_arg(*va, wint_t);
//                else
//                    va_arg(*va, int);
//                break;

//            /* specifiers */
//            case 's':
//                if (argument_type[0] == 'l' && argument_type[0] == '\0')
//                    va_arg(*va, wint_t);
//                else
//                    va_arg(*va, int);
//                break;


//        }
//    }
//}

//int ft_hdr_printf(FTABLE *FORT_RESTRICT table, const char* FORT_RESTRICT fmt, ...)
//{
//    int DUMMY_BUFFER_SZ = 1024;
//    char dummy_buffer[DUMMY_BUFFER_SZ];

//    char *fmt_copy = F_STRDUP(fmt);
//    if (fmt_copy == NULL)
//        return 0;

//    va_list va;
//    va_start(va, fmt);

//    char *pos = fmt_copy;
//    char *base_pos = fmt_copy;
//    while (*pos) {
//        pos = strchr(pos, FORT_COL_SEPARATOR);
//        if (pos != NULL) {
//            /* todo: do normal escape checking */
//            if (pos > base_pos && (*(pos - 1) == '\\')) { /* column separator is escaped */
//                ++pos;
//                continue;
//            } else { /* column separator is found and it is not escaped */
//                *(pos) = '\0';
//                ++pos;
//            }
//        }

//        while (1) {
//            va_list va_arg;
//            va_copy(va_arg, va);
//            int n = vsnprintf(dummy_buffer, DUMMY_BUFFER_SZ, base_pos, va_arg);

//            /* successful write */
//            if (n < DUMMY_BUFFER_SZ) {
//                forward_va_list(base_pos, &va);
//                fprintf(stderr, "%s\n", dummy_buffer);
//                break;
//            }
//        }

//        if (pos == NULL)
//            break;
//        base_pos = pos;
//    }

////    va_list va;
////    int written_cols = 0;



////    written_cols = vfprintf(stderr, fmt, va);

////    va_end(va);
////    va_start(va, fmt);
////    written_cols |= vfprintf(stderr, fmt, va);


////    va_end(va);

//    F_FREE(fmt_copy);
//    return 0;
//}



//int ft_hdr_printf(FTABLE *FORT_RESTRICT table, const char* FORT_RESTRICT fmt, ...)
//{
//    string_buffer_t *buffer = create_string_buffer(1024);
//    if (buffer == NULL)
//        return -1;

//    int cols_origin = number_of_columns_in_format_string(fmt);

//    while (1) {
//        va_list va;
//        va_start(va, fmt);
//        int virtual_sz = vsnprintf(buffer->str, buffer->str_sz, fmt, va);
//        va_end(va);
//        /* If error encountered */
//        if (virtual_sz == -1)
//            goto clear;

//        /* Successful write */
//        if (virtual_sz < buffer->str_sz)
//            break;

//        /* Otherwise buffer was too small, so incr. buffer size ant try again. */
//        if (!IS_SUCCESS(realloc_string_buffer_without_copy(buffer)))
//            goto clear;
//    }

//    int cols = number_of_columns_in_format_string(buffer->str);
//    if (cols == cols_origin) {

//        fort_row_t *row = create_row_from_string(buffer->str);
//        if (row == NULL) {
//            goto clear;
//        }

//        table->header = row;
//        return vector_size(row->cells);
//    }


//clear:
//    destroy_string_buffer(buffer);
//    return -1;
//}

int ft_hdr_printf(FTABLE *FORT_RESTRICT table, const char* FORT_RESTRICT fmt, ...)
{
    if (table == NULL)
        return -1;

    va_list va;
    va_start(va, fmt);
    fort_row_t *row = create_row_from_fmt_string(fmt, &va);
    va_end(va);

    if (row == NULL) {
        return -1;
    }

    if (table->header) {
        destroy_row(table->header);
    }
    table->header = row;
    return vector_size(row->cells);
}

int ft_row_printf(FTABLE *FORT_RESTRICT table, size_t row, const char* FORT_RESTRICT fmt, ...)
{
    if (table == NULL)
        return -1;

    va_list va;
    va_start(va, fmt);
    fort_row_t *new_row = create_row_from_fmt_string(fmt, &va);
    va_end(va);

    if (new_row == NULL) {
        return -1;
    }

    fort_row_t **cur_row_p = NULL;
    size_t sz = vector_size(table->rows);
    if (row >= sz) {
        size_t push_n = row - sz + 1;
        for (size_t i = 0; i < push_n; ++i) {
            fort_row_t *new_row = create_row();
            if (new_row == NULL)
                goto clear;

            if (IS_ERROR(vector_push(table->rows, &new_row))) {
                destroy_row(new_row);
                goto clear;
            }
        }
    }
    /* todo clearing pushed items in case of error */

    cur_row_p = (fort_row_t**)vector_at(table->rows, row);

    destroy_row(*cur_row_p);
    *cur_row_p = new_row;
    return vector_size(new_row->cells);

clear:
    destroy_row(new_row);
    return -1;
}



/*****************************************************************************
 *               STRING BUFFER
 * ***************************************************************************/
string_buffer_t* create_string_buffer(size_t sz)
{
    string_buffer_t *result = (string_buffer_t *)F_MALLOC(sizeof(string_buffer_t));
    if (result == NULL)
        return NULL;
    result->str = F_MALLOC(sz);
    if (result->str == NULL) {
        F_FREE(result);
        return NULL;
    }
    result->str_sz = sz;

    return result;
}

void destroy_string_buffer(string_buffer_t *buffer)
{
    if (buffer == NULL)
        return;
    F_FREE(buffer->str);
    buffer->str = NULL;
    F_FREE(buffer);
}

fort_status_t realloc_string_buffer_without_copy(string_buffer_t *buffer)
{
    assert(buffer);
    char *new_str = (char*)F_MALLOC(buffer->str_sz * 2);
    if (new_str == NULL) {
        return F_MEMORY_ERROR;
    }
    F_FREE(buffer->str);
    buffer->str = new_str;
    buffer->str_sz *= 2;
    return F_SUCCESS;
}

static fort_status_t fill_buffer_from_string(string_buffer_t *buffer, const char *str)
{
    assert(buffer);
    assert(str);

    size_t sz = strlen(str);
    char * copy = F_STRDUP(str);
    if (copy == NULL)
        return F_MEMORY_ERROR;

    while (sz >= buffer->str_sz) {
        int status = realloc_string_buffer_without_copy(buffer);
        if (!IS_SUCCESS(status)) {
            return status;
        }
    }
    F_FREE(buffer->str);
    buffer->str = copy;

    return F_SUCCESS;
}

/*****************************************************************************
 *               VECTOR IMPLEMENTATIONS
 * ***************************************************************************/

struct vector
{
    size_t m_size;
    void  *m_data;
    size_t m_capacity;
    size_t m_item_size;
};

static int vector_reallocate_(vector_t *vector, size_t new_capacity)
{
    assert(vector);
    assert(new_capacity > vector->m_capacity);

    size_t new_size = new_capacity * vector->m_item_size;
    vector->m_data = realloc(vector->m_data, new_size);
    if (vector->m_data == NULL)
        return -1;
    return 0;
}

/* ------------ Constructors & Destructors ----------------------------- */

static vector_t* create_vector(size_t item_size, size_t capacity)
{
    vector_t *vector = malloc(sizeof(vector_t));
    if (vector == NULL) {
        SYS_LOG_ERROR("Failed to allocate memory for asock vector");
        return NULL;
    }

    size_t init_size = MAX(item_size * capacity, 1);
    vector->m_data = malloc(init_size);
    if (vector->m_data == NULL) {
        SYS_LOG_ERROR("Failed to allocate memory for asock vector inern. buffer");
        free(vector);
        return NULL;
    }

    vector->m_size      = 0;
    vector->m_capacity  = capacity;
    vector->m_item_size = item_size;

    return vector;
}


static void destroy_vector(vector_t* vector)
{
    assert(vector);
    free(vector->m_data);
    free(vector);
}


/* ----------- Nonmodifying functions --------------------------------- */

static size_t vector_size(const vector_t* vector)
{
    assert(vector);
    return vector->m_size;
}


static size_t vector_capacity(const vector_t* vector)
{
    assert(vector);
    return vector->m_capacity;
}

static size_t vector_index_of(const vector_t* vector, const void *item)
{
    assert(vector);
    assert(item);

    for (size_t i = 0; i < vector->m_size; ++i) {
        void *data_pos = vector->m_data + i * vector->m_item_size;
        if (memcmp(data_pos, item, vector->m_item_size) == 0) {
            return i;
        }
    }
    return INVALID_VEC_INDEX;
}


/* ----------- Modifying functions ------------------------------------- */

static int vector_push (vector_t* vector, const void* item)
{
    assert(vector);
    assert(item);

    if (vector->m_size == vector->m_capacity) {
        if (vector_reallocate_(vector, vector->m_capacity * 2) == -1)
            return F_ERROR;
        vector->m_capacity = vector->m_capacity * 2;
    }

    ptrdiff_t deviation = vector->m_size * vector->m_item_size;
    memcpy(vector->m_data + deviation, item, vector->m_item_size);

    ++(vector->m_size);

    return F_SUCCESS;
}


static int vector_erase(vector_t *vector, size_t index)
{
    assert(vector);

    if (vector->m_size == 0 || index >= vector->m_size)
        return F_ERROR;

    memmove(vector->m_data + vector->m_item_size * index,
            vector->m_data + vector->m_item_size * (index + 1),
            (vector->m_size - 1 - index) * vector->m_item_size);
    vector->m_size--;
    return F_SUCCESS;
}


static void vector_clear(vector_t *vector)
{
    vector->m_size = 0;
}



static void *vector_at(vector_t *vector, size_t index)
{
    if (index >= vector->m_size)
        return NULL;

    return vector->m_data + index * vector->m_item_size;
}


#define FOR_EACH_(type, item, vector, index_name) \
    for (size_t index_name = 0; (item = *(type*)vector_at(vector, index_name)), (index_name < vector_size(vector));  ++index_name)

#define FOR_EACH(type, item, vector) \
    FOR_EACH_(type, item, vector, UNIQUE_NAME(i))

/*****************************************************************************
 *               ROW
 * ***************************************************************************/

static fort_row_t* create_row_from_string(const char *str)
{
    fort_row_t * row = create_row();
    if (row == NULL)
        return NULL;

    if (str == NULL)
        return row;

    char *str_copy = F_STRDUP(str);
    if (str_copy == NULL)
        goto clear;

    char *pos = str_copy;
    char *base_pos = str_copy;
    while (*pos) {
        pos = strchr(pos, FORT_COL_SEPARATOR);
        if (pos != NULL) {
            *(pos) = '\0';
            ++pos;
         }

        fort_cell_t *cell = create_cell();
        if (cell == NULL)
            goto clear;

        int status = fill_buffer_from_string(cell->str_buffer, base_pos);
        if (IS_ERROR(status)) {
            destroy_cell(cell);
            goto clear;
        }

        status = vector_push(row->cells, &cell);
        if (IS_ERROR(status)) {
            destroy_cell(cell);
            goto clear;
        }

        if (pos == NULL)
            break;
        base_pos = pos;
    }

    F_FREE(str_copy);
    return row;

clear:
    destroy_row(row);
    F_FREE(str_copy);
    return NULL;
}


static fort_row_t* create_row_from_fmt_string(const char* FORT_RESTRICT fmt, va_list *va_args)
{
    string_buffer_t *buffer = create_string_buffer(1024);
    if (buffer == NULL)
        return NULL;

    int cols_origin = number_of_columns_in_format_string(fmt);

    while (1) {
        va_list va;
        va_copy(va, *va_args);
        int virtual_sz = vsnprintf(buffer->str, buffer->str_sz, fmt, va);
        va_end(va);
        /* If error encountered */
        if (virtual_sz == -1)
            goto clear;

        /* Successful write */
        if (virtual_sz < buffer->str_sz)
            break;

        /* Otherwise buffer was too small, so incr. buffer size ant try again. */
        if (!IS_SUCCESS(realloc_string_buffer_without_copy(buffer)))
            goto clear;
    }

    int cols = number_of_columns_in_format_string(buffer->str);
    if (cols == cols_origin) {

        fort_row_t *row = create_row_from_string(buffer->str);
        if (row == NULL) {
            goto clear;
        }

        destroy_string_buffer(buffer);
        return row;
    }

    /* todo: add processing of cols != cols_origin */

clear:
    destroy_string_buffer(buffer);
    return NULL;
}


/*****************************************************************************
 *               TABLE
 * ***************************************************************************/

static int print_row(char *buffer, const fort_row_t *row)
{
    assert(row);
    ssize_t pos = 0;
    size_t v_size = vector_size(row->cells);
    for (size_t i = 0; i < v_size; ++i) {
        fort_cell_t* cell = *(fort_cell_t**)vector_at(row->cells, i);
        int deviation = sprintf(buffer + pos, "| %s", cell->str_buffer->str);
        pos += deviation;
    }
    return pos;
}

/*
 * Returns number of cells (rows * cols)
 */
static void table_body_sizes(const FTABLE *table, size_t *rows, size_t *cols)
{
    *rows = 0;
    *cols = 0;
    if (table && table->rows) {
        fort_row_t *row = NULL;
        FOR_EACH(fort_row_t*, row, table->rows) {
            size_t cols_in_row = columns_in_row(row);
            if (cols_in_row > *cols)
                *cols = cols_in_row;
        }
    }
}



/*
 * Returns geometry in characters
 */
static void table_geometry(const FTABLE *table, size_t *height, size_t *width)
{
    *height = 0;
    *width = 0;
    if (table) {
        size_t cols = 0;
        size_t rows = 0;
        table_sizes(table, &rows, &cols);
        if (table->header) {
            cols = MAX(cols, columns_in_row(table->header));
            ++rows;
        }
    }
}

char* ft_to_string(const FTABLE *FORT_RESTRICT table)
{
    assert(table);
    char *dummy = F_MALLOC(2056);

    fort_row_t *header = table->header;
    int dev = print_row(dummy, header);

    sprintf(dummy + dev , "\n");
    dev += 1;

    size_t rows = vector_size(table->rows);
    for (size_t i = 0; i < rows; ++i) {
        fort_row_t *row = *(fort_row_t**)vector_at(table->rows, i);
        dev += print_row(dummy + dev, row);
        sprintf(dummy + dev , "\n");
        dev += 1;
    }

    return dummy;
}






