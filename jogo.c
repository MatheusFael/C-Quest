#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 640

#define PLAYER_NAME_MAX 31
#define TOTAL_ROUNDS 12
#define MAX_RECORDS 128

typedef enum {
    SCREEN_MENU = 0,
    SCREEN_NAME_INPUT,
    SCREEN_QUIZ,
    SCREEN_RESULT,
    SCREEN_RECORDS,
    SCREEN_HELP
} Screen;

typedef struct {
    const char *prompt;
    const char *options[4];
    int correctOption;
} Question;

typedef struct {
    char name[PLAYER_NAME_MAX + 1];
    int score;
    int correctAnswers;
    char dateTime[20];
} RecordEntry;

typedef struct {
    HWND hwnd;
    HFONT titleFont;
    HFONT textFont;
    HFONT smallFont;
    HFONT monoFont;

    Screen screen;
    int menuIndex;

    char playerName[PLAYER_NAME_MAX + 1];
    int playerNameLen;

    int usedQuestions[TOTAL_ROUNDS];
    int usedCount;
    int currentQuestionIndex;
    int selectedOption;

    int roundIndex;
    int score;
    int guaranteed;
    int correctAnswers;

    char resultMessage[256];

    RecordEntry records[MAX_RECORDS];
    int recordCount;
} AppState;

static AppState gApp;

static const int gPrizeLadder[TOTAL_ROUNDS] = {
    1000, 2000, 5000, 10000, 20000, 30000,
    50000, 100000, 200000, 300000, 500000, 1000000
};

static const Question gQuestions[] = {
    {
        "Qual biblioteca deve ser incluida para usar strcmp?",
        {"<math.h>", "<string.h>", "<stdlib.h>", "<ctype.h>"},
        1
    },
    {
        "Qual especificador imprime um inteiro no printf?",
        {"%f", "%d", "%s", "%p"},
        1
    },
    {
        "Qual palavra reservada define uma constante em C?",
        {"const", "fixed", "final", "readonly"},
        0
    },
    {
        "Qual operador acessa o valor apontado por um ponteiro?",
        {"&", "*", "->", "::"},
        1
    },
    {
        "Qual funcao aloca memoria dinamica?",
        {"alloc", "new", "malloc", "reserve"},
        2
    },
    {
        "Em C, qual valor representa falso em expressoes logicas?",
        {"0", "-1", "NULL apenas", "qualquer numero"},
        0
    },
    {
        "Qual cabecalho e necessario para usar printf e scanf?",
        {"<stdio.h>", "<io.h>", "<conio.h>", "<string.h>"},
        0
    },
    {
        "Qual comando encerra um loop imediatamente?",
        {"continue", "return", "break", "stop"},
        2
    },
    {
        "Qual funcao abre um arquivo para leitura/escrita?",
        {"fopen", "fileopen", "openfile", "fcreate"},
        0
    },
    {
        "Qual tipo normalmente armazena um unico caractere?",
        {"str", "byte", "char", "character"},
        2
    },
    {
        "Qual e a forma correta de comparar strings em C?",
        {"a == b", "equals(a,b)", "strcmp(a,b) == 0", "a.compare(b)"},
        2
    },
    {
        "Qual palavra evita acesso externo a funcao global no arquivo?",
        {"private", "local", "static", "hidden"},
        2
    },
    {
        "Qual funcao libera memoria alocada por malloc?",
        {"delete", "release", "dispose", "free"},
        3
    },
    {
        "Qual estrutura repete enquanto condicao for verdadeira?",
        {"if", "for", "while", "switch"},
        2
    },
    {
        "Qual operador calcula o resto da divisao inteira?",
        {"/", "%", "//", "rem"},
        1
    },
    {
        "Qual cabecalho e usado para funcoes de tempo, como time()?",
        {"<clock.h>", "<chrono.h>", "<timer.h>", "<time.h>"},
        3
    },
    {
        "Qual e o valor de retorno convencional de main em sucesso?",
        {"1", "0", "-1", "42"},
        1
    },
    {
        "Qual e a assinatura mais comum do ponto de entrada em C?",
        {"void main()", "int main(void)", "main()", "start()"},
        1
    },
    {
        "Qual funcao le uma linha inteira de texto com limite?",
        {"gets", "fgets", "readline", "getlineC"},
        1
    },
    {
        "Qual operador obtem o endereco de uma variavel?",
        {"*", "#", "&", "@"},
        2
    }
};

#define QUESTION_COUNT ((int)(sizeof(gQuestions) / sizeof(gQuestions[0])))

static COLORREF rgb(int r, int g, int b)
{
    return RGB(r, g, b);
}

static void fill_rect(HDC hdc, int left, int top, int right, int bottom, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    RECT rc = { left, top, right, bottom };
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
}

static void draw_text_block(HDC hdc, const char *text, RECT rc, HFONT font, COLORREF color, UINT flags)
{
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextA(hdc, text, -1, &rc, flags);
    SelectObject(hdc, oldFont);
}

static int record_compare(const void *a, const void *b)
{
    const RecordEntry *ra = (const RecordEntry *)a;
    const RecordEntry *rb = (const RecordEntry *)b;

    if (rb->score != ra->score) {
        return rb->score - ra->score;
    }

    return rb->correctAnswers - ra->correctAnswers;
}

static void sanitize_player_name(char *name)
{
    int i;

    for (i = 0; name[i] != '\0'; i++) {
        if (name[i] == '|' || name[i] == '\n' || name[i] == '\r' || name[i] == ';') {
            name[i] = '_';
        }
    }
}

static void load_records(void)
{
    FILE *file = fopen("recordes.txt", "r");
    char line[256];

    gApp.recordCount = 0;

    if (file == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL && gApp.recordCount < MAX_RECORDS) {
        RecordEntry *entry = &gApp.records[gApp.recordCount];
        char *name;
        char *score;
        char *correct;
        char *date;

        name = strtok(line, "|");
        score = strtok(NULL, "|");
        correct = strtok(NULL, "|");
        date = strtok(NULL, "|\n\r");

        if (name != NULL && score != NULL && correct != NULL && date != NULL) {
            strncpy(entry->name, name, PLAYER_NAME_MAX);
            entry->name[PLAYER_NAME_MAX] = '\0';
            entry->score = atoi(score);
            entry->correctAnswers = atoi(correct);
            strncpy(entry->dateTime, date, sizeof(entry->dateTime) - 1);
            entry->dateTime[sizeof(entry->dateTime) - 1] = '\0';
            gApp.recordCount++;
            continue;
        }

        if (sscanf(line, "%dP - %31[^\n\r]", &entry->score, entry->name) == 2) {
            entry->correctAnswers = 0;
            strcpy(entry->dateTime, "formato antigo");
            gApp.recordCount++;
        }
    }

    fclose(file);

    if (gApp.recordCount > 1) {
        qsort(gApp.records, (size_t)gApp.recordCount, sizeof(RecordEntry), record_compare);
    }
}

static void save_record(void)
{
    FILE *file;
    char cleanName[PLAYER_NAME_MAX + 1];
    time_t now;
    struct tm localNow;
    char dateBuffer[20];

    strncpy(cleanName, gApp.playerName, PLAYER_NAME_MAX);
    cleanName[PLAYER_NAME_MAX] = '\0';
    sanitize_player_name(cleanName);

    now = time(NULL);
    localNow = *localtime(&now);
    strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d %H:%M", &localNow);

    file = fopen("recordes.txt", "a");
    if (file == NULL) {
        return;
    }

    fprintf(file, "%s|%d|%d|%s\n", cleanName, gApp.score, gApp.correctAnswers, dateBuffer);
    fclose(file);
}

static int question_already_used(int index)
{
    int i;

    for (i = 0; i < gApp.usedCount; i++) {
        if (gApp.usedQuestions[i] == index) {
            return 1;
        }
    }

    return 0;
}

static int pick_random_question(void)
{
    int tries;

    for (tries = 0; tries < 512; tries++) {
        int index = rand() % QUESTION_COUNT;
        if (!question_already_used(index)) {
            return index;
        }
    }

    return -1;
}

static void setup_next_question(void)
{
    int next = pick_random_question();

    if (next < 0) {
        snprintf(gApp.resultMessage, sizeof(gApp.resultMessage),
            "Banco de perguntas insuficiente para continuar.");
        gApp.screen = SCREEN_RESULT;
        return;
    }

    gApp.usedQuestions[gApp.usedCount++] = next;
    gApp.currentQuestionIndex = next;
    gApp.selectedOption = 0;
}

static void start_quiz(void)
{
    memset(gApp.usedQuestions, -1, sizeof(gApp.usedQuestions));
    gApp.usedCount = 0;
    gApp.roundIndex = 0;
    gApp.score = 0;
    gApp.guaranteed = 0;
    gApp.correctAnswers = 0;
    gApp.resultMessage[0] = '\0';

    setup_next_question();
    if (gApp.screen != SCREEN_RESULT) {
        gApp.screen = SCREEN_QUIZ;
    }
}

static void finish_game(int wonMillion)
{
    if (!wonMillion && gApp.score < gApp.guaranteed) {
        gApp.score = gApp.guaranteed;
    }

    if (wonMillion) {
        snprintf(gApp.resultMessage, sizeof(gApp.resultMessage),
            "Parabens, %s! Voce acertou tudo e ganhou 1.000.000!", gApp.playerName);
    } else {
        snprintf(gApp.resultMessage, sizeof(gApp.resultMessage),
            "%s, fim de jogo! Premio final: %d", gApp.playerName, gApp.score);
    }

    save_record();
    load_records();
    gApp.screen = SCREEN_RESULT;
}

static void process_answer(int option)
{
    const Question *q = &gQuestions[gApp.currentQuestionIndex];

    if (option == q->correctOption) {
        gApp.correctAnswers++;
        gApp.score = gPrizeLadder[gApp.roundIndex];

        if (gApp.roundIndex == 4 || gApp.roundIndex == 9) {
            gApp.guaranteed = gApp.score;
        }

        if (gApp.roundIndex == TOTAL_ROUNDS - 1) {
            finish_game(1);
            return;
        }

        gApp.roundIndex++;
        setup_next_question();
        return;
    }

    finish_game(0);
}

static void draw_gradient_background(HDC hdc)
{
    int y;
    for (y = 0; y < WINDOW_HEIGHT; y++) {
        int r = 10 + (y * 16 / WINDOW_HEIGHT);
        int g = 22 + (y * 40 / WINDOW_HEIGHT);
        int b = 42 + (y * 50 / WINDOW_HEIGHT);
        fill_rect(hdc, 0, y, WINDOW_WIDTH, y + 1, rgb(r, g, b));
    }
}

static void draw_menu(HDC hdc)
{
    static const char *menuItems[] = {
        "Iniciar Jogo",
        "Ver Recordes",
        "Como Jogar",
        "Sair"
    };
    RECT titleRect = { 0, 36, WINDOW_WIDTH, 110 };
    RECT subRect = { 0, 104, WINDOW_WIDTH, 148 };
    int i;

    draw_text_block(hdc, "C SHOW DO MILHAO", titleRect, gApp.titleFont, rgb(242, 238, 210),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    draw_text_block(hdc, "Desafie seu conhecimento em linguagem C", subRect, gApp.textFont, rgb(198, 220, 242),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    for (i = 0; i < 4; i++) {
        RECT itemRect = { WINDOW_WIDTH / 2 - 220, 185 + i * 90, WINDOW_WIDTH / 2 + 220, 245 + i * 90 };
        COLORREF boxColor = (gApp.menuIndex == i) ? rgb(242, 194, 71) : rgb(30, 57, 90);
        COLORREF textColor = (gApp.menuIndex == i) ? rgb(24, 26, 30) : rgb(225, 239, 252);

        fill_rect(hdc, itemRect.left, itemRect.top, itemRect.right, itemRect.bottom, boxColor);
        draw_text_block(hdc, menuItems[i], itemRect, gApp.textFont, textColor,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    {
        RECT hintRect = { 0, WINDOW_HEIGHT - 44, WINDOW_WIDTH, WINDOW_HEIGHT - 12 };
        draw_text_block(hdc, "Use Setas e Enter", hintRect, gApp.smallFont, rgb(180, 204, 229),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void draw_name_input(HDC hdc)
{
    RECT titleRect = { 0, 70, WINDOW_WIDTH, 140 };
    RECT boxRect = { WINDOW_WIDTH / 2 - 300, 250, WINDOW_WIDTH / 2 + 300, 320 };
    RECT hintRect = { 0, 350, WINDOW_WIDTH, 390 };
    char nameBuffer[PLAYER_NAME_MAX + 4];

    draw_text_block(hdc, "Digite o nome do jogador", titleRect, gApp.titleFont, rgb(242, 238, 210),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    fill_rect(hdc, boxRect.left, boxRect.top, boxRect.right, boxRect.bottom, rgb(20, 44, 70));

    snprintf(nameBuffer, sizeof(nameBuffer), "%s_", gApp.playerNameLen > 0 ? gApp.playerName : "");
    draw_text_block(hdc, nameBuffer, boxRect, gApp.textFont, rgb(236, 245, 255),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    draw_text_block(hdc, "Enter para iniciar | Esc para voltar", hintRect, gApp.smallFont, rgb(180, 204, 229),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void draw_quiz(HDC hdc)
{
    const Question *q = &gQuestions[gApp.currentQuestionIndex];
    RECT panelRect = { 60, 90, WINDOW_WIDTH - 60, 370 };
    RECT questionRect = { panelRect.left + 20, panelRect.top + 30, panelRect.right - 20, panelRect.bottom - 20 };
    int i;
    char header[160];

    snprintf(header, sizeof(header),
        "Jogador: %s | Pergunta %d/%d | Premio atual: %d | Garantido: %d",
        gApp.playerName,
        gApp.roundIndex + 1,
        TOTAL_ROUNDS,
        gApp.score,
        gApp.guaranteed);

    {
        RECT headRect = { 20, 22, WINDOW_WIDTH - 20, 64 };
        draw_text_block(hdc, header, headRect, gApp.smallFont, rgb(199, 220, 242),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    fill_rect(hdc, panelRect.left, panelRect.top, panelRect.right, panelRect.bottom, rgb(16, 42, 66));
    draw_text_block(hdc, q->prompt, questionRect, gApp.textFont, rgb(240, 247, 255),
        DT_LEFT | DT_TOP | DT_WORDBREAK);

    for (i = 0; i < 4; i++) {
        RECT optRect = { 90, 395 + i * 52, WINDOW_WIDTH - 90, 438 + i * 52 };
        char optionText[256];
        COLORREF box = (gApp.selectedOption == i) ? rgb(242, 194, 71) : rgb(26, 57, 87);
        COLORREF text = (gApp.selectedOption == i) ? rgb(22, 24, 28) : rgb(228, 238, 250);

        snprintf(optionText, sizeof(optionText), "%d) %s", i + 1, q->options[i]);
        fill_rect(hdc, optRect.left, optRect.top, optRect.right, optRect.bottom, box);
        draw_text_block(hdc, optionText, optRect, gApp.textFont, text,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    {
        RECT hintRect = { 20, WINDOW_HEIGHT - 35, WINDOW_WIDTH - 20, WINDOW_HEIGHT - 8 };
        draw_text_block(hdc, "Setas para selecionar, Enter para responder, teclas 1-4 como atalho",
            hintRect, gApp.smallFont, rgb(180, 204, 229),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void draw_result(HDC hdc)
{
    RECT titleRect = { 0, 68, WINDOW_WIDTH, 138 };
    RECT boxRect = { 120, 180, WINDOW_WIDTH - 120, 430 };
    RECT textRect = { boxRect.left + 30, boxRect.top + 40, boxRect.right - 30, boxRect.bottom - 100 };
    RECT footerRect = { boxRect.left + 30, boxRect.bottom - 85, boxRect.right - 30, boxRect.bottom - 35 };
    char scoreLine[128];

    draw_text_block(hdc, "Resultado", titleRect, gApp.titleFont, rgb(242, 238, 210),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    fill_rect(hdc, boxRect.left, boxRect.top, boxRect.right, boxRect.bottom, rgb(18, 47, 74));

    draw_text_block(hdc, gApp.resultMessage, textRect, gApp.textFont, rgb(234, 245, 255),
        DT_LEFT | DT_TOP | DT_WORDBREAK);

    snprintf(scoreLine, sizeof(scoreLine), "Acertos: %d | Premio final: %d", gApp.correctAnswers, gApp.score);
    draw_text_block(hdc, scoreLine, footerRect, gApp.textFont, rgb(242, 194, 71),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    {
        RECT hintRect = { 0, WINDOW_HEIGHT - 42, WINDOW_WIDTH, WINDOW_HEIGHT - 12 };
        draw_text_block(hdc, "Enter para voltar ao menu | R para ver recordes", hintRect,
            gApp.smallFont, rgb(180, 204, 229),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void draw_records(HDC hdc)
{
    RECT titleRect = { 0, 30, WINDOW_WIDTH, 90 };
    RECT boardRect = { 110, 100, WINDOW_WIDTH - 110, WINDOW_HEIGHT - 70 };
    int i;

    draw_text_block(hdc, "Ranking de Jogadores", titleRect, gApp.titleFont, rgb(242, 238, 210),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    fill_rect(hdc, boardRect.left, boardRect.top, boardRect.right, boardRect.bottom, rgb(16, 44, 70));

    {
        RECT hdrRect = { boardRect.left + 20, boardRect.top + 12, boardRect.right - 20, boardRect.top + 44 };
        draw_text_block(hdc, "Pos  Jogador                        Premio      Acertos   Data",
            hdrRect, gApp.monoFont, rgb(242, 194, 71), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    if (gApp.recordCount == 0) {
        RECT emptyRect = { boardRect.left + 20, boardRect.top + 80, boardRect.right - 20, boardRect.bottom - 20 };
        draw_text_block(hdc, "Nenhum recorde encontrado.", emptyRect, gApp.textFont, rgb(225, 239, 252),
            DT_LEFT | DT_TOP | DT_WORDBREAK);
    } else {
        int limit = gApp.recordCount < 10 ? gApp.recordCount : 10;

        for (i = 0; i < limit; i++) {
            RECT lineRect = {
                boardRect.left + 20,
                boardRect.top + 52 + i * 34,
                boardRect.right - 20,
                boardRect.top + 82 + i * 34
            };
            char row[256];

            snprintf(row, sizeof(row), "%02d   %-30s %-10d  %-8d  %s",
                i + 1,
                gApp.records[i].name,
                gApp.records[i].score,
                gApp.records[i].correctAnswers,
                gApp.records[i].dateTime);

            draw_text_block(hdc, row, lineRect, gApp.monoFont, rgb(225, 239, 252),
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }

    {
        RECT hintRect = { 0, WINDOW_HEIGHT - 42, WINDOW_WIDTH, WINDOW_HEIGHT - 12 };
        draw_text_block(hdc, "Esc ou Enter para voltar", hintRect, gApp.smallFont, rgb(180, 204, 229),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void draw_help(HDC hdc)
{
    const char *helpText =
        "1. Informe seu nome e inicie o jogo.\n"
        "2. Cada rodada mostra uma pergunta sobre linguagem C.\n"
        "3. Escolha a resposta com Setas + Enter, ou com teclas 1 a 4.\n"
        "4. Errou? Voce leva apenas o valor garantido (rodadas 5 e 10).\n"
        "5. Acerte as 12 perguntas para ganhar 1.000.000!\n\n"
        "Dica: estude ponteiros, bibliotecas padrao, strings e memoria dinamica.";
    RECT titleRect = { 0, 40, WINDOW_WIDTH, 100 };
    RECT boxRect = { 110, 120, WINDOW_WIDTH - 110, WINDOW_HEIGHT - 80 };
    RECT txtRect = { boxRect.left + 24, boxRect.top + 24, boxRect.right - 24, boxRect.bottom - 24 };

    draw_text_block(hdc, "Como Jogar", titleRect, gApp.titleFont, rgb(242, 238, 210),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    fill_rect(hdc, boxRect.left, boxRect.top, boxRect.right, boxRect.bottom, rgb(16, 44, 70));
    draw_text_block(hdc, helpText, txtRect, gApp.textFont, rgb(225, 239, 252), DT_LEFT | DT_TOP | DT_WORDBREAK);

    {
        RECT hintRect = { 0, WINDOW_HEIGHT - 42, WINDOW_WIDTH, WINDOW_HEIGHT - 12 };
        draw_text_block(hdc, "Esc ou Enter para voltar", hintRect, gApp.smallFont, rgb(180, 204, 229),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void render(HDC hdc)
{
    draw_gradient_background(hdc);

    switch (gApp.screen) {
    case SCREEN_MENU:
        draw_menu(hdc);
        break;
    case SCREEN_NAME_INPUT:
        draw_name_input(hdc);
        break;
    case SCREEN_QUIZ:
        draw_quiz(hdc);
        break;
    case SCREEN_RESULT:
        draw_result(hdc);
        break;
    case SCREEN_RECORDS:
        draw_records(hdc);
        break;
    case SCREEN_HELP:
        draw_help(hdc);
        break;
    default:
        break;
    }
}

static void handle_menu_key(WPARAM key)
{
    if (key == VK_UP) {
        gApp.menuIndex = (gApp.menuIndex + 3) % 4;
    } else if (key == VK_DOWN) {
        gApp.menuIndex = (gApp.menuIndex + 1) % 4;
    } else if (key == VK_RETURN) {
        if (gApp.menuIndex == 0) {
            gApp.playerName[0] = '\0';
            gApp.playerNameLen = 0;
            gApp.screen = SCREEN_NAME_INPUT;
        } else if (gApp.menuIndex == 1) {
            load_records();
            gApp.screen = SCREEN_RECORDS;
        } else if (gApp.menuIndex == 2) {
            gApp.screen = SCREEN_HELP;
        } else {
            PostQuitMessage(0);
        }
    }
}

static void handle_name_keydown(WPARAM key)
{
    if (key == VK_BACK) {
        if (gApp.playerNameLen > 0) {
            gApp.playerNameLen--;
            gApp.playerName[gApp.playerNameLen] = '\0';
        }
    } else if (key == VK_ESCAPE) {
        gApp.screen = SCREEN_MENU;
    } else if (key == VK_RETURN) {
        if (gApp.playerNameLen > 0) {
            start_quiz();
        }
    }
}

static void handle_quiz_key(WPARAM key)
{
    if (key == VK_UP) {
        gApp.selectedOption = (gApp.selectedOption + 3) % 4;
    } else if (key == VK_DOWN) {
        gApp.selectedOption = (gApp.selectedOption + 1) % 4;
    } else if (key == VK_RETURN) {
        process_answer(gApp.selectedOption);
    } else if (key >= '1' && key <= '4') {
        gApp.selectedOption = (int)(key - '1');
        process_answer(gApp.selectedOption);
    } else if (key == VK_ESCAPE) {
        gApp.screen = SCREEN_MENU;
    }
}

static void handle_result_key(WPARAM key)
{
    if (key == VK_RETURN || key == VK_ESCAPE) {
        gApp.screen = SCREEN_MENU;
    } else if (key == 'R' || key == 'r') {
        gApp.screen = SCREEN_RECORDS;
    }
}

static void handle_common_back(WPARAM key)
{
    if (key == VK_ESCAPE || key == VK_RETURN) {
        gApp.screen = SCREEN_MENU;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        gApp.hwnd = hwnd;
        gApp.titleFont = CreateFontA(46, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FF_SWISS, "Segoe UI");
        gApp.textFont = CreateFontA(26, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FF_SWISS, "Segoe UI");
        gApp.smallFont = CreateFontA(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FF_SWISS, "Segoe UI");
        gApp.monoFont = CreateFontA(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FIXED_PITCH, "Consolas");

        gApp.screen = SCREEN_MENU;
        gApp.menuIndex = 0;
        load_records();
        break;

    case WM_CHAR:
        if (gApp.screen == SCREEN_NAME_INPUT) {
            if (wParam >= 32 && wParam <= 126 && gApp.playerNameLen < PLAYER_NAME_MAX) {
                gApp.playerName[gApp.playerNameLen++] = (char)wParam;
                gApp.playerName[gApp.playerNameLen] = '\0';
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        break;

    case WM_KEYDOWN:
        if (gApp.screen == SCREEN_MENU) {
            handle_menu_key(wParam);
        } else if (gApp.screen == SCREEN_NAME_INPUT) {
            handle_name_keydown(wParam);
        } else if (gApp.screen == SCREEN_QUIZ) {
            handle_quiz_key(wParam);
        } else if (gApp.screen == SCREEN_RESULT) {
            handle_result_key(wParam);
        } else if (gApp.screen == SCREEN_RECORDS || gApp.screen == SCREEN_HELP) {
            handle_common_back(wParam);
        }

        InvalidateRect(hwnd, NULL, FALSE);
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        render(hdc);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DESTROY:
        DeleteObject(gApp.titleFont);
        DeleteObject(gApp.textFont);
        DeleteObject(gApp.smallFont);
        DeleteObject(gApp.monoFont);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSA wc;
    RECT wr = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    HWND hwnd;
    MSG msg;

    (void)hPrevInstance;
    (void)lpCmdLine;

    srand((unsigned int)time(NULL));

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "CQuizMilhaoClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&wc)) {
        return 0;
    }

    AdjustWindowRect(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        "Quiz do Milhao em C",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wr.right - wr.left,
        wr.bottom - wr.top,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
