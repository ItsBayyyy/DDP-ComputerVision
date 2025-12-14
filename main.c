#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>

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
#define C_RED    "\x1b[31m"
#define C_GREEN  "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_BLUE   "\x1b[34m"
#define C_CYAN   "\x1b[36m"
#define C_MAGENTA "\x1b[35m"

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
int BOX_COLORS[COLOR_COUNT][3] = {{255,0,0}, {0,255,0}, {0,0,255}, {255,255,0}};

typedef enum { SIZE_SMALL, SIZE_MEDIUM, SIZE_LARGE, SIZE_COUNT } SizeCategory;
const char *SIZE_NAMES[] = {"Kecil", "Sedang", "Besar"};

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

typedef enum { SCENE_LANDSCAPE, SCENE_DOCUMENT, SCENE_ILLUSTRATION, SCENE_MIXED } SceneType;
const char *SCENE_NAMES[] = {"Pemandangan Alam", "Dokumen/Teks", "Ilustrasi", "Campuran"};
SceneType current_scene = SCENE_MIXED;
char scene_reason[128];

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

/* ================= VISUALIZATION UTILS ================= */
void set_pixel_safe(unsigned char *img, int w, int h, int x, int y, int r, int g, int b, int thickness) {
    for(int ty = -thickness/2; ty <= thickness/2; ty++) {
        for(int tx = -thickness/2; tx <= thickness/2; tx++) {
            int ny = y + ty;
            int nx = x + tx;
            if(nx >= 0 && nx < w && ny >= 0 && ny < h) {
                int idx = (ny * w + nx) * 3;
                img[idx] = r;
                img[idx+1] = g;
                img[idx+2] = b;
            }
        }
    }
}

void draw_bounding_boxes(unsigned char *img, int w, int h) {
    printf(C_YELLOW "\n[VISUALIZATION] Menggambar %d kotak deteksi..." C_RESET "\n", kb_count);
    
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
            printf(C_GREEN "[VISUALIZATION] Gambar hasil disimpan sebagai 'result.jpg'" C_RESET "\n");
            remove("result.ppm"); 
        } else {
            printf(C_GREEN "[VISUALIZATION] Gambar hasil disimpan sebagai 'result.ppm'" C_RESET "\n");
        }
    } else {
        printf(C_RED "[ERROR] Gagal menyimpan file result!" C_RESET "\n");
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
    INT_GREETING, INT_IDENTITY 
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

    float max_score = 0;
    IntentType detected = INT_UNKNOWN;

    if(score_count > max_score) { max_score = score_count; detected = INT_COUNT; }
    if(score_where > max_score) { max_score = score_where; detected = INT_WHERE; }
    if(score_desc > max_score)  { max_score = score_desc;  detected = INT_DESCRIBE; }
    if(score_stats > max_score) { max_score = score_stats; detected = INT_STATS; }
    if(score_greet > max_score) { max_score = score_greet; detected = INT_GREETING; }
    if(score_ident > max_score) { max_score = score_ident; detected = INT_IDENTITY; }
    
    if(strstr(input, "klasifikasi") || strstr(input, "tipe")) { detected = INT_SCENE; max_score = 0.9f; }

    res.type = detected;
    res.confidence = max_score > 1.0f ? 0.99f : max_score;
    
    if(res.confidence > 0.3f && res.confidence < 0.65f) {
        if(detected == INT_COUNT) strcpy(res.suggestion, "menghitung jumlah objek");
        else if(detected == INT_WHERE) strcpy(res.suggestion, "mencari lokasi/posisi");
        else if(detected == INT_STATS) strcpy(res.suggestion, "melihat statistik");
        else if(detected == INT_GREETING) strcpy(res.suggestion, "menyapa saya");
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

    printf("\n" C_BOLD "=== ASTRAL SYSTEM V3 SIAP (" C_GREEN "ONLINE" C_RESET C_BOLD ") ===" C_RESET "\n");
    printf("Deteksi Awal  : " C_MAGENTA "%s" C_RESET "\n", SCENE_NAMES[current_scene]);
    printf("Status Sensor : %d objek teridentifikasi.\n\n", kb_count);
    printf(C_CYAN "Tips: Coba tanyakan 'Ada berapa benda merah?' atau 'Jelaskan gambar ini'.\n" C_RESET);

    char input[256];
    char temp_msg[512];
    
    while(1) {
        printf("\n" C_BOLD "Kamu > " C_RESET);
        if(!fgets(input, 256, stdin)) break;
        input[strcspn(input, "\n")] = 0;
        if(strcmp(input, "exit")==0 || strcmp(input, "keluar")==0) break;

        NLUResult intent = process_input(input);

        if(intent.confidence < sys_config.confidence_threshold) {
            if (strlen(intent.suggestion) > 0) {
                 int rnd = rand() % 2;
                 if(rnd == 0) sprintf(temp_msg, "Hmm, sinyal kurang jelas. Maksud kamu ingin %s? (Ketik ulang jika ya)\n", intent.suggestion);
                 else         sprintf(temp_msg, "Saya menebak kamu ingin %s, benar tidak? Coba perjelas lagi.\n", intent.suggestion);
                 print_typing(C_YELLOW "Astral > " C_RESET, temp_msg);
                 continue;
            } else {
                 int rnd = rand() % 3;
                 if(rnd == 0) print_typing(C_RED "Astral > " C_RESET, "Maaf, sirkuit bahasa saya belum mengenali kalimat itu. Coba tanya tentang warna atau posisi.\n");
                 else if(rnd == 1) print_typing(C_RED "Astral > " C_RESET, "Waduh, saya gagal paham. Bisa gunakan kata kunci lain? Contoh: 'hitung merah' atau 'dimana biru'.\n");
                 else print_typing(C_RED "Astral > " C_RESET, "Input tidak terdefinisi dalam database saya. Coba perintah yang lebih spesifik.\n");
                 continue;
            }
        }

        char response[1024] = ""; 
        char prefix[64];
        int rnd = rand() % 3; 
        
        switch(intent.type) {
            case INT_GREETING:
                if(rnd==0) sprintf(response, "Halo! Astral siap membantu. Ada objek tertentu yang ingin kamu cari di gambar ini?");
                else if(rnd==1) sprintf(response, "Hai! Saya Astral, sedang memantau visual gambar. Mau tanya statistik warna atau jumlah objek?");
                else sprintf(response, "Selamat datang. Saya Astral. Silakan ajukan pertanyaan tentang gambar.");
                break;

            case INT_IDENTITY:
                sprintf(response, "Perkenalkan, saya adalah Astral (Visual Intelligence V3). Saya dilengkapi algoritma Iterative Flood Fill dan Naive Bayes untuk memahami gambar dan bahasamu.");
                break;

            case INT_COUNT:
                {
                    int count = 0;
                    for(int i=0; i<kb_count; i++) {
                        int match_color = (intent.entity_color == COLOR_UNKNOWN) || (kb_objects[i].color == intent.entity_color);
                        int match_size  = (intent.entity_size == SIZE_COUNT) || (kb_objects[i].size_cat == intent.entity_size);
                        if(match_color && match_size) count++;
                    }

                    char obj_desc[64] = "objek";
                    if(intent.entity_color != COLOR_UNKNOWN) sprintf(obj_desc, "objek berwarna %s", COLOR_NAMES[intent.entity_color]);
                    
                    if(count == 0) {
                        if(rnd==0) sprintf(response, "Wah, setelah scanning pixel demi pixel, saya tidak menemukan %s sama sekali di sini.", obj_desc);
                        else sprintf(response, "Nihil. Sensor saya tidak menangkap adanya %s di dalam frame gambar ini.", obj_desc);
                    } else {
                        if(rnd==0) sprintf(response, "Sip! Hasil pemindaian menunjukkan ada total %d %s yang terdeteksi.", count, obj_desc);
                        else if(rnd==1) sprintf(response, "Tertangkap radar! Saya menemukan %d %s yang tersebar di gambar.", count, obj_desc);
                        else sprintf(response, "Menurut perhitungan saya, terdapat %d %s saat ini.", count, obj_desc);
                    }
                }
                break;

            case INT_WHERE:
                if(intent.entity_color == COLOR_UNKNOWN) {
                    sprintf(response, "Tunggu dulu, objek warna apa yang mau dilacak posisinya? Coba sebutkan warnanya.");
                } else {
                    if(rnd==0) sprintf(response, "Oke, mari kita lacak koordinat %s. Berikut data posisinya:", COLOR_NAMES[intent.entity_color]);
                    else sprintf(response, "Siap. Menampilkan lokasi koordinat X,Y untuk semua objek %s:", COLOR_NAMES[intent.entity_color]);
                }
                break;

            case INT_STATS:
                if(rnd==0) sprintf(response, "Ini dia laporan statistik distribusi warna yang saya kumpulkan dari gambar:");
                else sprintf(response, "Membuka database statistik... Berikut persentase area untuk setiap warna:");
                break;

            case INT_SCENE:
                sprintf(response, "Berdasarkan analisis fitur warna dan objek, saya mengklasifikasikan ini sebagai %s. Alasannya: %s.", SCENE_NAMES[current_scene], scene_reason);
                break;

            case INT_DESCRIBE:
                sprintf(response, "Secara garis besar, ini adalah gambar %s. Data sensor saya menunjukkan dominasi Hijau %.1f%% dan Biru %.1f%%, dengan total %d objek terpisah.", 
                    SCENE_NAMES[current_scene], img_features.color_pct[GREEN], img_features.color_pct[BLUE], kb_count);
                break;

            default:
                sprintf(response, "Hmm, saya menangkap suara tapi belum paham maksudnya. Bisa diulangi dengan kata lain?");
        }

        if(response[0]) {
            sprintf(prefix, C_CYAN "Astral > " C_RESET);
            print_typing(prefix, response);
            
            if(intent.type == INT_WHERE && intent.entity_color != COLOR_UNKNOWN) {
                int limit = 0;
                int found_any = 0;
                for(int i=0; i<kb_count; i++) {
                    if(kb_objects[i].color == intent.entity_color) {
                        printf("       • [%s] ditemukan di koordinat pusat (%d, %d)\n", SIZE_NAMES[kb_objects[i].size_cat], kb_objects[i].cx, kb_objects[i].cy);
                        found_any = 1;
                        if(++limit > 5) { printf("       ... (dan %d lainnya)\n", img_features.obj_count[intent.entity_color]-5); break; }
                        sleep_ms(30);
                    }
                }
                if(!found_any) printf("       (Tidak ada data lokasi karena objek tidak ditemukan)\n");
            } else if(intent.type == INT_STATS) {
                for(int i=0; i<COLOR_COUNT; i++) {
                    printf("       • %-6s: %5.2f%% area (Total: %d objek)\n", COLOR_NAMES[i], img_features.color_pct[i], img_features.obj_count[i]);
                    sleep_ms(30);
                }
            }
        }
    }

    if(temp_flag) remove(ppm_file);
    free(img_data); free(mask); free(labels);
    return 0;
}