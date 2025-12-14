#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <conio.h> 

/* ================= KONFIGURASI SISTEM ================= */
typedef struct {
    int min_area;
    int dilate_iter;
    float confidence_threshold;
    int with_result; 
    char input_file[256];
} Config;

Config sys_config = { .min_area = 200, .dilate_iter = 3, .confidence_threshold = 0.5, .with_result = 0 };

/* ================= UTILITAS OS & UI ================= */
#include <windows.h>
void sleep_ms(int ms) { Sleep(ms); }

#define C_RESET  "\x1b[0m"
#define C_BOLD   "\x1b[1m"
#define C_DIM    "\x1b[2m"
#define C_RED    "\x1b[31m"
#define C_GREEN  "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_BLUE   "\x1b[34m"
#define C_CYAN   "\x1b[36m"
#define C_MAGENTA "\x1b[35m"
#define C_WHITE  "\x1b[37m"

void enable_ansi_support() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= 0x0004; 
    SetConsoleMode(hOut, dwMode);
}

void gotoxy(int x, int y) {
    printf("\x1b[%d;%dH", y, x);
}

void clear_screen() {
    printf("\x1b[2J\x1b[H");
}

/* ================= FITUR VISUAL (Konsep User) ================= */
void animate_crt_off() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    int h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    int w = csbi.srWindow.Right - csbi.srWindow.Left + 1;

    for (int i = 0; i < h / 2; i++) {
        gotoxy(1, i);
        for(int x=0; x<w; x++) putchar(' '); 
        gotoxy(1, h - i);
        for(int x=0; x<w; x++) putchar(' '); 
        sleep_ms(10);
    }

    gotoxy(1, h/2);
    for(int i=0; i<w; i++) putchar('-'); 
    sleep_ms(100);
    
    for(int i = 0; i < w/2; i++) {
        gotoxy(1+i, h/2); putchar(' ');
        gotoxy(w-i, h/2); putchar(' ');
        sleep_ms(5);
    }
    
    gotoxy(w/2, h/2); printf(C_WHITE "*" C_RESET);
    sleep_ms(200);
    gotoxy(w/2, h/2); printf(" ");
    sleep_ms(200);
    clear_screen();
}

void print_glitch(const char *text) {
    int len = strlen(text);
    char chars[] = "!@#$%^&*()_+-=[]{}|;':,./<>?";
    
    printf(C_RED);
    for(int i=0; i<len; i++) {
        if(rand() % 5 == 0) {
            putchar(chars[rand() % (sizeof(chars)-1)]);
            fflush(stdout);
            sleep_ms(rand() % 50);
            printf("\b"); 
        }
        putchar(text[i]);
        fflush(stdout);
        sleep_ms(15 + (rand() % 20)); 
    }
    printf(C_RESET "\n");
}

void animate_hex_dump() {
    printf(C_DIM C_GREEN);
    for(int i=0; i<8; i++) { 
        printf("0x%08X  ", rand() * rand());
        for(int j=0; j<8; j++) {
            printf("%02X ", rand() % 256);
        }
        printf(" |");
        for(int j=0; j<8; j++) {
            char c = (rand() % 94) + 32;
            putchar(c);
        }
        printf("|\n");
        sleep_ms(20); 
    }
    printf(C_RESET "Analysis Complete.\n");
    sleep_ms(200);
}

void get_confidence_bar(float conf, char *buffer) {
    int bars = 10;
    int filled = (int)(conf * bars);
    
    char color[16];
    if(conf > 0.8) strcpy(color, C_GREEN);
    else if(conf > 0.5) strcpy(color, C_YELLOW);
    else strcpy(color, C_RED);
    
    strcpy(buffer, color);
    strcat(buffer, "[");
    for(int i=0; i<bars; i++) {
        if(i < filled) strcat(buffer, "|");
        else strcat(buffer, " ");
    }
    strcat(buffer, "] ");
    
    char percent[16];
    sprintf(percent, "%.0f%%", conf * 100);
    strcat(buffer, percent);
    strcat(buffer, C_RESET);
}

void print_typing(const char *prefix, const char *text) {
    printf("%s", prefix);
    while (*text) {
        putchar(*text++);
        fflush(stdout);
        sleep_ms(15);
    }
    printf("\n");
}

void show_spinner(const char *msg) {
    const char *frames[] = {"|", "/", "-", "\\"};
    for(int i=0; i<8; i++) { 
        printf("\r" C_YELLOW "%s %s..." C_RESET, frames[i%4], msg);
        fflush(stdout);
        sleep_ms(80);
    }
    printf("\r" C_GREEN "✔ %s Selesai!          " C_RESET "\n", msg);
}

/* ================= ALGORITMA FUZZY STRING (LEVENSHTEIN) ================= */
int min3(int a, int b, int c) {
    int m = a;
    if (b < m) m = b;
    if (c < m) m = c;
    return m;
}

int levenshtein_distance(const char *s1, const char *s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    int matrix[len1 + 1][len2 + 1];
    int i, j;

    for (i = 0; i <= len1; i++) matrix[i][0] = i;
    for (j = 0; j <= len2; j++) matrix[0][j] = j;

    for (i = 1; i <= len1; i++) {
        for (j = 1; j <= len2; j++) {
            int cost = (tolower(s1[i - 1]) == tolower(s2[j - 1])) ? 0 : 1;
            matrix[i][j] = min3(
                matrix[i - 1][j] + 1,
                matrix[i][j - 1] + 1,
                matrix[i - 1][j - 1] + cost
            );
        }
    }
    return matrix[len1][len2];
}

/* ================= STRUKTUR DATA VISION ================= */
typedef enum { RED, GREEN, BLUE, YELLOW, COLOR_COUNT, COLOR_UNKNOWN } ColorEnum;
const char *COLOR_NAMES[] = {"Merah", "Hijau", "Biru", "Kuning"};
const char *COLOR_ANSI[]  = {C_RED, C_GREEN, C_BLUE, C_YELLOW}; 
int BOX_COLORS[COLOR_COUNT][3] = {{255,0,0}, {0,255,0}, {0,0,255}, {255,255,0}};

typedef enum { SIZE_SMALL, SIZE_MEDIUM, SIZE_LARGE, SIZE_COUNT } SizeCategory;
const char *SIZE_NAMES[] = {"Kecil", "Sedang", "Besar"};
char SIZE_SYMBOLS[] = {'.', 'o', 'O'};

typedef struct {
    int id;
    ColorEnum color;
    SizeCategory size_cat;
    int area;
    int min_x, min_y, max_x, max_y;
    int cx, cy;
} DetectedObject;

#define MAX_OBJECTS 1000 
DetectedObject kb_objects[MAX_OBJECTS];
int kb_count = 0;

typedef struct {
    float color_pct[COLOR_COUNT]; 
    int obj_count[COLOR_COUNT];   
    int total_objects;
} FeatureVector;

FeatureVector img_features;

int img_width_g, img_height_g;

typedef enum { SCENE_LANDSCAPE, SCENE_DOCUMENT, SCENE_ILLUSTRATION, SCENE_MIXED } SceneType;
const char *SCENE_NAMES[] = {"Pemandangan Alam", "Dokumen/Teks", "Ilustrasi", "Campuran"};
SceneType current_scene = SCENE_MIXED;
char scene_reason[128];

/* ================= FITUR VISUAL RADAR ================= */
void draw_radar(ColorEnum target_color) {
    int rw = 40; 
    int rh = 12; 
    char grid[12][41];

    for(int y=0; y<rh; y++) {
        for(int x=0; x<rw; x++) grid[y][x] = ' ';
        grid[y][rw] = '\0';
    }

    for(int x=0; x<rw; x++) { grid[0][x] = '-'; grid[rh-1][x] = '-'; }
    for(int y=0; y<rh; y++) { grid[y][0] = '|'; grid[y][rw-1] = '|'; }
    grid[0][0]='+'; grid[0][rw-1]='+'; grid[rh-1][0]='+'; grid[rh-1][rw-1]='+';

    int found_count = 0;
    for(int i=0; i<kb_count; i++) {
        if(kb_objects[i].color == target_color) {
            int rx = 1 + (kb_objects[i].cx * (rw - 3)) / img_width_g;
            int ry = 1 + (kb_objects[i].cy * (rh - 3)) / img_height_g;
            
            if(rx < 1) rx = 1; if(rx > rw-2) rx = rw-2;
            if(ry < 1) ry = 1; if(ry > rh-2) ry = rh-2;

            grid[ry][rx] = SIZE_SYMBOLS[kb_objects[i].size_cat]; 
            found_count++;
        }
    }

    printf(C_CYAN "\n   [ RADAR VISUAL: %s ]\n" C_RESET, COLOR_NAMES[target_color]);
    for(int y=0; y<rh; y++) {
        printf("   %s%s%s\n", COLOR_ANSI[target_color], grid[y], C_RESET);
    }
    printf("   Legenda: (.)Kecil (o)Sedang (O)Besar\n\n");
}

/* ================= COMPUTER VISION CORE ================= */
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

void rgb_to_hsv(int r, int g, int b, float *h, float *s, float *v) {
    float rf=r/255.0f, gf=g/255.0f, bf=b/255.0f;
    float mx=MAX(rf,MAX(gf,bf)), mn=MIN(rf,MIN(gf,bf));
    float d=mx-mn;
    *v=mx;
    *s=(mx==0)?0:d/mx;
    if(mx==mn) *h=0;
    else if(mx==rf) *h=60*fmod(((gf-bf)/d),6);
    else if(mx==gf) *h=60*(((bf-rf)/d)+2);
    else *h=60*(((rf-gf)/d)+4);
    if(*h<0) *h+=360;
}

int check_color(float h, float s, float v, ColorEnum c) {
    switch(c) {
        case RED: 
            return ((h<20 || h>340) && s>0.3 && v>0.2);
        case GREEN: 
            return (h>45 && h<165 && s>0.2 && v>0.15); 
        case BLUE: 
            return (h>=165 && h<270 && s>0.2 && v>0.2);
        case YELLOW: 
            return (h>=25 && h<=45 && s>0.4 && v>0.4);
        default: return 0;
    }
}

void morphology_pass(int *mask, int w, int h, int type) {
    int *tmp = malloc(w*h*sizeof(int));
    memcpy(tmp, mask, w*h*sizeof(int));
    
    for(int y=1; y<h-1; y++) {
        for(int x=1; x<w-1; x++) {
            int idx = y*w+x;
            int hit = 0;
            if (type == 0) { 
                hit = 1;
                for(int ky=-1; ky<=1; ky++)
                    for(int kx=-1; kx<=1; kx++)
                        if(!mask[(y+ky)*w+(x+kx)]) { hit=0; goto end_kernel; }
            } else { 
                hit = 0;
                for(int ky=-1; ky<=1; ky++)
                    for(int kx=-1; kx<=1; kx++)
                        if(mask[(y+ky)*w+(x+kx)]) { hit=1; goto end_kernel; }
            }
            end_kernel:
            tmp[idx] = hit;
        }
    }
    memcpy(mask, tmp, w*h*sizeof(int));
    free(tmp);
}

typedef struct { int x, y; } Point;

void flood_fill_iterative(int *mask, int *labels, int w, int h, int start_x, int start_y, int id, int *stats) {
    int capacity = w * h;
    Point *stack = malloc(capacity * sizeof(Point));
    if(!stack) { printf("Memory Error!\n"); return; }
    
    int top = 0;
    stack[top++] = (Point){start_x, start_y};
    labels[start_y*w + start_x] = id; 
    stats[4]++; 
    if(start_x < stats[0]) stats[0] = start_x;
    if(start_y < stats[1]) stats[1] = start_y;
    if(start_x > stats[2]) stats[2] = start_x;
    if(start_y > stats[3]) stats[3] = start_y;

    while(top > 0) {
        Point p = stack[--top];
        int x = p.x, y = p.y;
        int dx[] = {1, -1, 0, 0};
        int dy[] = {0, 0, 1, -1};

        for(int i=0; i<4; i++) {
            int nx = x + dx[i], ny = y + dy[i];
            if(nx >= 0 && nx < w && ny >= 0 && ny < h) {
                int n_idx = ny*w + nx;
                if(mask[n_idx] == 1 && labels[n_idx] == 0) {
                    labels[n_idx] = id;
                    stack[top++] = (Point){nx, ny};
                    stats[4]++;
                    if(nx < stats[0]) stats[0] = nx;
                    if(ny < stats[1]) stats[1] = ny;
                    if(nx > stats[2]) stats[2] = nx;
                    if(ny > stats[3]) stats[3] = ny;
                }
            }
        }
    }
    free(stack);
}

/* ================= ASCII ART ANIMATION SYSTEM ================= */

const char *ASCII_CHARS = " .:-=+*#%@";

void animate_ascii_scan(unsigned char *img, int w, int h, int skip_intro) {
    int console_w = 80;
    int scale_x = w / console_w;
    if (scale_x < 1) scale_x = 1;
    int scale_y = scale_x * 2; 

    int console_h = h / scale_y;
    
    if(!skip_intro) {
        printf("\n" C_BOLD C_CYAN "=== MEMULAI VISUALISASI DIGITAL ===" C_RESET "\n");
        sleep_ms(500);
        printf("\x1b[2J\x1b[H"); 
    }
    
    int start_row = 2; 
    int line_delay = (console_h > 0) ? (3000 / console_h) : 10;
    if(skip_intro) line_delay = 5; 
    
    for (int y = 0; y < console_h; y++) {
        gotoxy(1, start_row + y); 
        
        for (int x = 0; x < console_w; x++) {
            int img_x = x * scale_x;
            int img_y = y * scale_y;
            
            if (img_x >= w) img_x = w - 1;
            if (img_y >= h) img_y = h - 1;
            
            int idx = (img_y * w + img_x) * 3;
            unsigned char r = img[idx];
            unsigned char g = img[idx+1];
            unsigned char b = img[idx+2];
            
            int gray = (int)(0.299*r + 0.587*g + 0.114*b);
            int char_idx = gray * 9 / 255; 
            
            putchar(ASCII_CHARS[char_idx]);
        }
        
        printf(C_GREEN " < SCANNING..." C_RESET);
        
        fflush(stdout);
        sleep_ms(line_delay);
    }
    
    gotoxy(console_w + 2, start_row + console_h - 1);
    printf("              ");

    if(!skip_intro) {
        gotoxy(1, start_row + console_h + 2);
        printf(C_YELLOW "Mengidentifikasi Objek..." C_RESET);
        sleep_ms(1000);

        for (int i = 0; i < kb_count; i++) {
            int cx_console = kb_objects[i].cx / scale_x;
            int cy_console = kb_objects[i].cy / scale_y;
            
            int draw_y = start_row + cy_console;
            int draw_x = cx_console;
            
            const char *col_code = COLOR_ANSI[kb_objects[i].color];
            
            gotoxy(draw_x, draw_y);
            printf("%s[%d]" C_RESET, col_code, i+1);

            gotoxy(1, start_row + console_h + 1);
            printf(C_CYAN "> Deteksi ID #%d: %s (%s) di grid [%d,%d]   " C_RESET, 
                i+1, COLOR_NAMES[kb_objects[i].color], SIZE_NAMES[kb_objects[i].size_cat], cx_console, cy_console);
            
            sleep_ms(200); 
        }

        gotoxy(1, start_row + console_h + 3);
        printf(C_GREEN "Visualisasi Selesai." C_RESET "\n");
        sleep_ms(1000);
    }
}

void set_pixel_safe(unsigned char *img, int w, int h, int x, int y, int r, int g, int b, int thick) {
    int start = -thick / 2;
    int end = (thick + 1) / 2;
    
    for(int dy = start; dy < end; dy++) {
        for(int dx = start; dx < end; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if(nx >= 0 && nx < w && ny >= 0 && ny < h) {
                int idx = (ny * w + nx) * 3;
                img[idx] = (unsigned char)r;
                img[idx+1] = (unsigned char)g;
                img[idx+2] = (unsigned char)b;
            }
        }
    }
}

void draw_bounding_boxes(unsigned char *img, int w, int h) {
    for(int i=0; i<kb_count; i++) {
        int *col = BOX_COLORS[kb_objects[i].color];
        int x1 = kb_objects[i].min_x;
        int y1 = kb_objects[i].min_y;
        int x2 = kb_objects[i].max_x;
        int y2 = kb_objects[i].max_y;
        
        for(int x=x1; x<=x2; x++) {
            set_pixel_safe(img, w, h, x, y1, col[0], col[1], col[2], 2);
            set_pixel_safe(img, w, h, x, y2, col[0], col[1], col[2], 2);
        }

        for(int y=y1; y<=y2; y++) {
            set_pixel_safe(img, w, h, x1, y, col[0], col[1], col[2], 2);
            set_pixel_safe(img, w, h, x2, y, col[0], col[1], col[2], 2);
        }
    }

    FILE *fp = fopen("result.ppm", "wb");
    if(fp) {
        fprintf(fp, "P6\n%d %d\n255\n", w, h);
        fwrite(img, 1, w*h*3, fp);
        fclose(fp);
        
        int ret = system("magick result.ppm result.jpg");
        if(ret == 0) {
            // remove("result.ppm"); 
        }
    }
}

/* ================= DECISION & ML ================= */
void run_kmeans_clustering() {
    if(kb_count < 3) return;
    float centroids[3] = {999999, 0, 0};
    for(int i=0; i<kb_count; i++) {
        if(kb_objects[i].area < centroids[0]) centroids[0] = kb_objects[i].area;
        if(kb_objects[i].area > centroids[2]) centroids[2] = kb_objects[i].area;
    }
    centroids[1] = (centroids[0] + centroids[2]) / 2;

    int iter = 0;
    while(iter < 20) {
        float sums[3] = {0}; int counts[3] = {0}; int changes = 0;
        for(int i=0; i<kb_count; i++) {
            float dist_min = 99999999.0f; int cluster = 0;
            for(int k=0; k<3; k++) {
                float dist = fabsf(kb_objects[i].area - centroids[k]);
                if(dist < dist_min) { dist_min = dist; cluster = k; }
            }
            if(kb_objects[i].size_cat != cluster) changes++;
            kb_objects[i].size_cat = cluster;
            sums[cluster] += kb_objects[i].area; counts[cluster]++;
        }
        for(int k=0; k<3; k++) if(counts[k]>0) centroids[k] = sums[k]/counts[k];
        if(changes == 0) break;
        iter++;
    }
}

void classify_scene() {
    float total_color_area = 0;
    for(int i=0; i<COLOR_COUNT; i++) total_color_area += img_features.color_pct[i];

    int is_nature = (img_features.color_pct[GREEN] > 15.0f || img_features.color_pct[BLUE] > 15.0f);
    int is_clean = (img_features.color_pct[RED] < 10.0f);

    if(is_nature && is_clean) {
        current_scene = SCENE_LANDSCAPE;
        strcpy(scene_reason, "Dominasi unsur alam (Hijau/Biru) terdeteksi kuat");
    }
    else if(img_features.color_pct[RED] > 10 && img_features.color_pct[YELLOW] > 10 && img_features.total_objects < 50) {
        current_scene = SCENE_ILLUSTRATION;
        strcpy(scene_reason, "Distribusi warna cerah & kontras tinggi");
    }
    else if(img_features.total_objects > 10 && total_color_area < 15.0f) {
        current_scene = SCENE_DOCUMENT;
        strcpy(scene_reason, "Dominasi background & objek kecil (teks)");
    }
    else {
        current_scene = SCENE_MIXED;
        strcpy(scene_reason, "Komposisi warna/objek tidak spesifik");
    }
}

/* ================= NLU ENGINE ================= */
typedef enum { 
    INT_UNKNOWN, INT_COUNT, INT_WHERE, INT_DESCRIBE, INT_STATS, INT_SCENE,
    INT_GREETING, INT_IDENTITY, 
    INT_HELP, INT_TIME, INT_SCAN, INT_EXIT 
} IntentType;

typedef struct {
    ColorEnum last_color;
    SizeCategory last_size;
    IntentType last_intent;
    int has_context;
} SessionMemory;

SessionMemory user_session = { .has_context = 0 };

typedef struct {
    IntentType type;
    float confidence;
    ColorEnum entity_color;
    SizeCategory entity_size;
    char suggestion[64]; 
} NLUResult;

const char *VOCAB_COUNT[] = {"berapa", "jumlah", "banyak", "total", "hitung", "ada"};
const char *VOCAB_WHERE[] = {"dimana", "posisi", "letak", "koordinat", "sebelah", "cari"};
const char *VOCAB_DESC[]  = {"jelaskan", "deskripsi", "apa", "gambar", "ceritakan"};
const char *VOCAB_STATS[] = {"statistik", "data", "persen", "luas", "area", "info"};
const char *VOCAB_GREET[] = {"halo", "hi", "test", "pagi", "siang", "sore", "malam", "woi", "hai"};
const char *VOCAB_IDENT[] = {"siapa", "kamu", "nama", "bot", "program", "sistem"};
const char *VOCAB_HELP[]  = {"bantuan", "help", "tolong", "menu", "perintah", "cmd", "panduan"};
const char *VOCAB_TIME[]  = {"jam", "waktu", "tanggal", "status", "uptime"};
const char *VOCAB_SCAN[]  = {"scan", "pindai", "ulangi", "refresh", "cek", "lihat"};
const char *VOCAB_EXIT[]  = {"exit", "keluar", "bye", "tutup", "quit", "selesai"};

float calculate_bayes_score(char *input, const char **vocab, int vocab_size) {
    float matches = 0.0f;
    char temp[256]; strcpy(temp, input);
    char *token = strtok(temp, " ?.,");
    int total_words = 0;
    while(token) {
        total_words++;
        for(int i=0; i<vocab_size; i++) {
            if(strcasecmp(token, vocab[i]) == 0) matches += 1.0f;
            else {
                int dist = levenshtein_distance(token, vocab[i]);
                int len = strlen(vocab[i]);
                if ((len > 4 && dist <= 2) || (len <= 4 && dist <= 1)) matches += 0.8f;
            }
        }
        token = strtok(NULL, " ?.,");
    }
    if(total_words == 0) return 0.0f;
    return matches / total_words + (matches > 0 ? 0.3f : 0);
}

NLUResult process_input(char *raw_input) {
    NLUResult res = {INT_UNKNOWN, 0.0f, COLOR_UNKNOWN, SIZE_COUNT, ""};
    char input[256];
    strcpy(input, raw_input);
    for(int i=0; input[i]; i++) input[i] = tolower(input[i]);

    if(strstr(input, "merah") || strstr(input, "mrh")) res.entity_color = RED;
    if(strstr(input, "hijau") || strstr(input, "hijo")) res.entity_color = GREEN;
    if(strstr(input, "biru") || strstr(input, "blue"))  res.entity_color = BLUE;
    if(strstr(input, "kuning") || strstr(input, "knng")) res.entity_color = YELLOW;
    
    if(strstr(input, "kecil")) res.entity_size = SIZE_SMALL;
    if(strstr(input, "besar")) res.entity_size = SIZE_LARGE;

    if(strstr(input, "itu") || strstr(input, "tersebut") || strstr(input, "tadi") || strstr(input, "nya")) {
        if(user_session.has_context && res.entity_color == COLOR_UNKNOWN) 
            res.entity_color = user_session.last_color;
    }

    float score_count = calculate_bayes_score(input, VOCAB_COUNT, 6);
    float score_where = calculate_bayes_score(input, VOCAB_WHERE, 6);
    float score_desc  = calculate_bayes_score(input, VOCAB_DESC, 5);
    float score_stats = calculate_bayes_score(input, VOCAB_STATS, 6);
    float score_greet = calculate_bayes_score(input, VOCAB_GREET, 9);
    float score_ident = calculate_bayes_score(input, VOCAB_IDENT, 6);
    float score_help  = calculate_bayes_score(input, VOCAB_HELP, 7);
    float score_time  = calculate_bayes_score(input, VOCAB_TIME, 5);
    float score_scan  = calculate_bayes_score(input, VOCAB_SCAN, 6);
    float score_exit  = calculate_bayes_score(input, VOCAB_EXIT, 6);

    float max_score = 0;
    IntentType detected = INT_UNKNOWN;

    if(score_count > max_score) { max_score = score_count; detected = INT_COUNT; }
    if(score_where > max_score) { max_score = score_where; detected = INT_WHERE; }
    if(score_desc > max_score)  { max_score = score_desc;  detected = INT_DESCRIBE; }
    if(score_stats > max_score) { max_score = score_stats; detected = INT_STATS; }
    if(score_greet > max_score) { max_score = score_greet; detected = INT_GREETING; }
    if(score_ident > max_score) { max_score = score_ident; detected = INT_IDENTITY; }
    if(score_help > max_score)  { max_score = score_help;  detected = INT_HELP; }
    if(score_time > max_score)  { max_score = score_time;  detected = INT_TIME; }
    if(score_scan > max_score)  { max_score = score_scan;  detected = INT_SCAN; }
    if(score_exit > max_score)  { max_score = score_exit;  detected = INT_EXIT; }
    
    if(strstr(input, "klasifikasi") || strstr(input, "tipe")) { detected = INT_SCENE; max_score = 0.9f; }

    res.type = detected;
    res.confidence = max_score > 1.0f ? 0.99f : max_score;
    
    if(res.confidence > 0.3f && res.confidence < 0.65f) {
        if(detected == INT_COUNT) strcpy(res.suggestion, "menghitung jumlah objek");
        else if(detected == INT_WHERE) strcpy(res.suggestion, "mencari lokasi/posisi");
        else if(detected == INT_STATS) strcpy(res.suggestion, "melihat statistik");
        else if(detected == INT_HELP)  strcpy(res.suggestion, "melihat menu bantuan");
    }

    if(res.entity_color != COLOR_UNKNOWN) user_session.last_color = res.entity_color;
    if(detected != INT_UNKNOWN && detected != INT_GREETING && detected != INT_IDENTITY) 
        user_session.last_intent = detected;
    user_session.has_context = 1;
    return res;
}

/* ================= MAIN PIPELINE ================= */
int main(int argc, char *argv[]) {
    srand(time(NULL)); 
    enable_ansi_support(); 

    #ifdef _WIN32
        SetConsoleOutputCP(65001);
    #endif

    if(argc < 2) {
        printf("Usage: %s <image.jpg/ppm> [--min-area N] [--with-result]\n", argv[0]);
        return 1;
    }
    strcpy(sys_config.input_file, argv[1]);
    for(int i=2; i<argc; i++) {
        if(strcmp(argv[i], "--min-area")==0) sys_config.min_area = atoi(argv[++i]);
        if(strcmp(argv[i], "--with-result")==0) sys_config.with_result = 1;
    }

    char ppm_file[256] = "temp_proc.ppm";
    int temp_flag = 0;
    
    if(!strstr(sys_config.input_file, ".ppm")) {
        char cmd[512];
        sprintf(cmd, "magick \"%s\" -compress none %s", sys_config.input_file, ppm_file);
        printf(C_YELLOW "Mengkonversi Gambar (High Definition)..." C_RESET "\n");
        system(cmd);
        temp_flag = 1;
    } else {
        strcpy(ppm_file, sys_config.input_file);
    }

    FILE *fp = fopen(ppm_file, "rb");
    if(!fp) { printf(C_RED "Gagal membuka file. Pastikan path benar!\n"); return 1; }
    char header[3]; int w, h, max_val;
    fscanf(fp, "%s %d %d %d", header, &w, &h, &max_val);
    img_width_g = w; img_height_g = h; 
    
    unsigned char *img_data = malloc(w*h*3);
    if(strcmp(header, "P6")==0) { fgetc(fp); fread(img_data, 1, w*h*3, fp); }
    else { for(int i=0; i<w*h; i++) fscanf(fp, "%hhu %hhu %hhu", &img_data[i*3], &img_data[i*3+1], &img_data[i*3+2]); }
    fclose(fp);

    int *mask = calloc(w*h, sizeof(int));
    int *labels = calloc(w*h, sizeof(int));
    int total_pixels = w*h;
    
    show_spinner("Memproses Citra & Ekstraksi Fitur");

    for(int c=0; c<COLOR_COUNT; c++) {
        memset(mask, 0, w*h*sizeof(int));
        memset(labels, 0, w*h*sizeof(int));
        int raw_color_count = 0;

        for(int i=0; i<total_pixels; i++) {
            float H, S, V;
            rgb_to_hsv(img_data[i*3], img_data[i*3+1], img_data[i*3+2], &H, &S, &V);
            if(check_color(H, S, V, c)) {
                mask[i] = 1;
                raw_color_count++;
            }
        }
        img_features.color_pct[c] = ((float)raw_color_count / total_pixels) * 100.0f;

        morphology_pass(mask, w, h, 0); 
        for(int k=0; k<sys_config.dilate_iter; k++) morphology_pass(mask, w, h, 1); 

        int obj_id = 0;
        for(int y=0; y<h; y++) {
            for(int x=0; x<w; x++) {
                int idx = y*w+x;
                if(mask[idx] && labels[idx] == 0) {
                    if (kb_count >= MAX_OBJECTS) break;

                    int stats[5] = {w, h, 0, 0, 0}; 
                    flood_fill_iterative(mask, labels, w, h, x, y, ++obj_id, stats);
                    
                    if(stats[4] >= sys_config.min_area) {
                        kb_objects[kb_count].id = kb_count + 1;
                        kb_objects[kb_count].color = c;
                        kb_objects[kb_count].area = stats[4];
                        kb_objects[kb_count].min_x = stats[0]; kb_objects[kb_count].min_y = stats[1];
                        kb_objects[kb_count].max_x = stats[2]; kb_objects[kb_count].max_y = stats[3];
                        kb_objects[kb_count].cx = (stats[0]+stats[2])/2;
                        kb_objects[kb_count].cy = (stats[1]+stats[3])/2;
                        
                        img_features.obj_count[c]++;
                        img_features.total_objects++;
                        kb_count++;
                    }
                }
            }
        }
    }

    run_kmeans_clustering();
    classify_scene();

    if(sys_config.with_result) {
        draw_bounding_boxes(img_data, w, h);
    }

    animate_ascii_scan(img_data, w, h, 0);

    printf("\n" C_BOLD "=== ASTRAL AI SIAP (" C_GREEN "ONLINE" C_RESET C_BOLD ") ===" C_RESET "\n");
    printf("Deteksi Awal  : " C_MAGENTA "%s" C_RESET "\n", SCENE_NAMES[current_scene]);
    printf("Status Sensor : %d objek teridentifikasi.\n", kb_count);
    printf("Ketik " C_YELLOW "'bantuan'" C_RESET " untuk melihat perintah.\n\n");

    char input[256];
    char temp_msg[512];
    char conf_bar[64];
    
    while(1) {
        printf("\n" C_BOLD "Kamu > " C_RESET);
        if(!fgets(input, 256, stdin)) break;
        input[strcspn(input, "\n")] = 0;
        
        NLUResult intent = process_input(input);
        
        if(intent.type == INT_EXIT) {
            int r = rand() % 3;
            if(r==0) printf(C_YELLOW "Astral > Shutting down systems..." C_RESET "\n");
            else if(r==1) printf(C_YELLOW "Astral > Menyimpan sesi dan keluar..." C_RESET "\n");
            else printf(C_YELLOW "Astral > Bye. Sensor offline." C_RESET "\n");
            sleep_ms(500);
            animate_crt_off();
            break;
        }
        
        if(intent.type == INT_UNKNOWN && intent.confidence < 0.3) {
             print_glitch("ERROR: UNKNOWN INTENT DETECTED... PARSING FAILED.");
        }
        
        if(intent.type == INT_SCENE || intent.type == INT_DESCRIBE) {
            animate_hex_dump();
        }

        get_confidence_bar(intent.confidence, conf_bar);
        printf(C_DIM "Confidence: %s" C_RESET "\n", conf_bar);

        if(intent.confidence < sys_config.confidence_threshold) {
            if (strlen(intent.suggestion) > 0) {
                 int rnd_conf = rand() % 4;
                 switch(rnd_conf) {
                     case 0:
                        sprintf(temp_msg, "Sinyal input terdistorsi noise. Apakah Anda bermaksud %s?\n", intent.suggestion);
                        break;
                     case 1:
                        sprintf(temp_msg, "Maaf, modul bahasa saya agak ragu. Maksud perintah tadi %s bukan?\n", intent.suggestion);
                        break;
                     case 2:
                        sprintf(temp_msg, "Hmm, data tidak lengkap. Namun pola algoritmanya mirip dengan %s. Benarkah?\n", intent.suggestion);
                        break;
                     case 3:
                        sprintf(temp_msg, "Input samar-samar. Mencoba menebak... apakah arahnya ke %s?\n", intent.suggestion);
                        break;
                 }
                 print_typing(C_YELLOW "Astral > " C_RESET, temp_msg);
                 continue;
            } else {
                 print_glitch("DATA CORRUPTED. COBA LAGI.");
                 continue;
            }
        }

        char response[1024] = ""; 
        char prefix[64];
        
        switch(intent.type) {
            case INT_GREETING: {
                int r = rand() % 5;
                if(r==0) sprintf(response, "Halo! Radar visual aktif dan siap.");
                else if(r==1) sprintf(response, "Astral online. Siap memproses query visual.");
                else if(r==2) sprintf(response, "Koneksi stabil. Ada yang bisa saya bantu analisa?");
                else if(r==3) sprintf(response, "Sistem `Astral` mendengarkan. Silakan.");
                else sprintf(response, "Hai. Sensor optik saya siap digunakan.");
                break;
            }

            case INT_IDENTITY: {
                int r = rand() % 3;
                if(r==0) sprintf(response, "Saya Astral. Spesialis computer vision & NLU.");
                else if(r==1) sprintf(response, "Sistem Astral. Dilengkapi modul Hex Dump dan Radar Visual.");
                else sprintf(response, "Saya adalah asisten visual digital Anda.");
                break;
            }

            case INT_HELP:
                printf(C_CYAN "=== MENU BANTUAN ASTRAL ===" C_RESET "\n");
                printf("1. " C_YELLOW "Hitung" C_RESET "   : 'Ada berapa kotak merah?', 'Total objek?'\n");
                printf("2. " C_YELLOW "Lokasi" C_RESET "   : 'Dimana yang biru?', 'Cari posisi hijau'\n");
                printf("3. " C_YELLOW "Info" C_RESET "     : 'Jelaskan gambar', 'Statistik warna'\n");
                printf("4. " C_YELLOW "Sistem" C_RESET "   : 'Cek status', 'Scan ulang', 'Keluar'\n");
                strcpy(response, "Silakan pilih perintah di atas.");
                break;

            case INT_TIME: {
                time_t now; time(&now);
                struct tm *local = localtime(&now);
                int r = rand() % 3;
                if(r==0) sprintf(response, "Waktu Sistem: %02d:%02d. Sensor berjalan nominal.", local->tm_hour, local->tm_min);
                else if(r==1) sprintf(response, "Log Timestamp: %02d:%02d WIB. All systems go.", local->tm_hour, local->tm_min);
                else sprintf(response, "Sekarang pukul %02d:%02d. Menunggu instruksi selanjutnya.", local->tm_hour, local->tm_min);
                break;
            }

            case INT_SCAN: {
                int r = rand() % 3;
                if(r==0) printf(C_YELLOW "Menginisialisasi ulang scanner visual..." C_RESET "\n");
                else if(r==1) printf(C_YELLOW "Re-calibrating optic sensors..." C_RESET "\n");
                else printf(C_YELLOW "Memperbarui buffer citra..." C_RESET "\n");
                
                animate_ascii_scan(img_data, w, h, 1); 
                
                if(r==0) sprintf(response, "Scan ulang selesai. Data visual telah diperbarui.");
                else if(r==1) sprintf(response, "Kalibrasi tuntas. Representasi visual mutakhir.");
                else sprintf(response, "Buffer refreshed. Siap untuk analisa baru.");
                break;
            }

            case INT_COUNT:
                {
                    int count = 0;
                    for(int i=0; i<kb_count; i++) {
                        int match_color = (intent.entity_color == COLOR_UNKNOWN) || (kb_objects[i].color == intent.entity_color);
                        int match_size  = (intent.entity_size == SIZE_COUNT) || (kb_objects[i].size_cat == intent.entity_size);
                        if(match_color && match_size) count++;
                    }

                    char obj_desc[64] = "objek";
                    if(intent.entity_color != COLOR_UNKNOWN) sprintf(obj_desc, "objek %s", COLOR_NAMES[intent.entity_color]);
                    
                    int r = rand() % 4;
                    if(r==0) sprintf(response, "Hasil scan: %d %s terdeteksi.", count, obj_desc);
                    else if(r==1) sprintf(response, "Sensor menangkap keberadaan %d %s.", count, obj_desc);
                    else if(r==2) sprintf(response, "Total kalkulasi: ada %d %s di dalam frame.", count, obj_desc);
                    else sprintf(response, "Saya menemukan %d %s dalam area pantauan.", count, obj_desc);
                }
                break;

            case INT_WHERE:
                if(intent.entity_color == COLOR_UNKNOWN) {
                    sprintf(response, "Warna apa yang mau ditampilkan di Radar?");
                } else {
                    int r = rand() % 3;
                    if(r==0) sprintf(response, "Mengaktifkan subsistem radar untuk objek %s...", COLOR_NAMES[intent.entity_color]);
                    else if(r==1) sprintf(response, "Memplot koordinat objek %s ke layar...", COLOR_NAMES[intent.entity_color]);
                    else sprintf(response, "Melacak posisi sinyal %s... Visualisasi diaktifkan.", COLOR_NAMES[intent.entity_color]);
                }
                break;

            case INT_STATS: {
                int r = rand() % 3;
                if(r==0) sprintf(response, "Membuka log statistik distribusi warna...");
                else if(r==1) sprintf(response, "Mengakses database visual... Ini data komposisinya.");
                else sprintf(response, "Menampilkan histogram warna dari citra yang diproses.");
                break;
            }

            case INT_SCENE: {
                int r = rand() % 3;
                if(r==0) sprintf(response, "Klasifikasi Scene: %s. (%s)", SCENE_NAMES[current_scene], scene_reason);
                else if(r==1) sprintf(response, "Analisa lingkungan: %s. Indikator: %s.", SCENE_NAMES[current_scene], scene_reason);
                else sprintf(response, "Algoritma mendeteksi pola %s berdasarkan %s.", SCENE_NAMES[current_scene], scene_reason);
                break;
            }

            case INT_DESCRIBE: {
                int r = rand() % 3;
                if(r==0)
                    sprintf(response, "Scene %s. Dominasi: Hijau %.1f%%, Biru %.1f%%.", 
                        SCENE_NAMES[current_scene], img_features.color_pct[GREEN], img_features.color_pct[BLUE]);
                else if(r==1)
                    sprintf(response, "Analisa gambar: Tipe %s dengan %d total objek terisolasi.",
                        SCENE_NAMES[current_scene], kb_count);
                else
                    sprintf(response, "Deskripsi singkat: %s. Terdeteksi %d objek mencolok.", 
                        SCENE_NAMES[current_scene], kb_count);
                break;
            }

            default:
                sprintf(response, "Query tidak valid. Coba ulangi.");
        }

        if(response[0]) {
            sprintf(prefix, C_CYAN "Astral > " C_RESET);
            print_typing(prefix, response);
            
            if(intent.type == INT_WHERE && intent.entity_color != COLOR_UNKNOWN) {
                draw_radar(intent.entity_color);
            } 
            else if(intent.type == INT_STATS) {
                for(int i=0; i<COLOR_COUNT; i++) {
                    printf("       • %-6s: %5.2f%% area\n", COLOR_NAMES[i], img_features.color_pct[i]);
                    sleep_ms(30);
                }
            }
        }
    }

    if(temp_flag) remove(ppm_file);
    free(img_data); free(mask); free(labels);
    return 0;
}