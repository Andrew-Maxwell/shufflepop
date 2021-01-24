#include <iostream>
#include <raylib.h>
#include <cstdint>
#include <math.h>
#include <cassert>
#include <stdlib.h>
#include <time.h>
#include <unordered_map>

#define ROWS 9
#define COLS 5
#define TILEX 48
#define FONTSIZE 64
#define TILEY 52
#define SPACE 10
#define GRIDX (TILEX + SPACE)
#define GRIDY (TILEY + SPACE)
#define NUMSUITES 4
#define NUMCOLORS 4
#define HEALTHBARWIDTH 20
#define SCREENWIDTH (COLS * GRIDX + SPACE + HEALTHBARWIDTH)
#define SCREENHEIGHT (ROWS * GRIDY + SPACE)

#define GREEN {0x28, 0x96, 0x5A, 0xFF}
#define RED {0xDA, 0x3E, 0x52, 0xFF}
#define WHITE {0xEE, 0xEB, 0xD3, 0xFF}
#define YELLOW {0xEE, 0xC3, 0x3F, 0xFF}
#define BLUE {0x2E, 0x29, 0x6C, 0xFF}
#define BLACK {0x19, 0x15, 0x16, 0xFF}

#define BASESCORE 10
#define POWERSCORE 5
#define ACCELERATION 0.00055
#define SPEEDBOOST 0.003
#define SPEEDSCORE 50

using namespace std;

Font font;
const int ERROR = 'E';
const int STAR = 0x2217;
const int GONE = ' ';
const int SUITES[] = {0x2660, 0x2663, 0x2665, 0x2666};
const Color COLORS[] = {RED, YELLOW, BLACK, BLUE};
const int MOVES[] = {'<', '>'};
const int DIE = 0x2680;
const int SPEED = '+';

bool tap() {
    return IsKeyPressed(KEY_SPACE) ||
    IsKeyPressed(KEY_ENTER) ||
    IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ||
    IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) ||
    GetGestureDetected() == GESTURE_TAP;
}

bool operator==( const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

int mod(int a, int b) {
    return ((a % b) + b) % b;
}

struct Tile {
    int display;
    Color color;

    Tile() {
        display = ERROR;
        color = PURPLE;
    }

    Tile(int newDisplay, Color newColor = WHITE) : display(newDisplay), color(newColor) {}

    void draw(int x, float y, bool selected = false, bool last = false) {
        if (!isGone()) {
            Rectangle rec = {GRIDX * x + SPACE, GRIDY * y + SPACE, TILEX, TILEY};
            if (last) {
                DrawCircle(rec.x + TILEX / 2, rec.y + TILEY / 2, TILEX / 2 + 3, BLACK);
                DrawCircle(rec.x + TILEX / 2, rec.y + TILEY / 2, TILEX / 2, WHITE);
            }
            else {
                if (isSuite()) {
                    DrawRectangleRounded(rec, 0.2, 5, WHITE);
                    if (selected) {
                        DrawRectangleRoundedLines(rec, 0.2, 5, 3, BLACK);
                    }
                }
                else {
                    DrawRectangleRounded(rec, 0.2, 5, BLACK);
                    if (selected) {
                        DrawRectangleRoundedLines(rec, 0.2, 5, 3, WHITE);
                    }
                }
            }
            DrawTextCodepoint(font, display, {GRIDX * x + 7 + SPACE, GRIDY * y + 6}, FONTSIZE, color);
        }
    }

    bool isStar() { return display == STAR; }

    bool isGone() { return display == GONE; }

    bool isSuite() { return (display >= SUITES[0] && display <= SUITES[3]) || isStar(); }

    bool isMovement() { return display == MOVES[0] || display == MOVES[1]; }

    bool isSpeed() { return display == SPEED; }

    bool isDie() { return display >= DIE && display <= DIE + 5; }

    bool match(Tile& other) {
        assert(isSuite() && other.isSuite());
        return display == other.display || color == other.color || isStar() || other.isStar();
    }
};

struct Bag {
    int suites;
    int stars;
    int movements;
    int speeds;
    int dice;
    int total;

    Bag(int suitesProportion, int starsProportion, int movementsProportion, int speedProportion, int diceProportion) {
        suites = suitesProportion;
        stars = suites + starsProportion;
        movements = movementsProportion + stars;
        speeds = speedProportion + movements;
        dice = speeds + diceProportion;
        total = dice;
    }

    Tile grab() {
        int choice = rand() % total + 1;
        if (choice <= suites) {
            return Tile(SUITES[rand() % NUMSUITES], COLORS[rand() % NUMCOLORS]);
        }
        else if (choice <= stars) {
            return Tile(STAR, BLACK);
        }
        else if (choice <= movements) {
            return Tile(MOVES[rand() % 2]);
        }
        else if (choice <= speeds) {
            return Tile(SPEED);
        }
        else if (choice <= dice) {
            return Tile(DIE + rand() % 6);
        }
        return Tile();
    }
};

struct Row {
    Tile tiles[COLS];

    Tile& operator[](int col) {
        return tiles[mod(col, COLS)];
    }
};

struct Board {
    Row rows[ROWS];

    Row& operator[](int row) {
        return rows[mod(row, ROWS)];
    }
};

struct Message {
    string text;
    Bag bag;
    Row examples;

    Message(string newText, Bag newBag, Tile a = Tile(GONE), Tile b = Tile(GONE), Tile c = Tile(GONE), Tile d = Tile(GONE), Tile e = Tile(GONE)) :
        text(newText), bag(newBag) {
            examples[0] = a;
            examples[1] = b;
            examples[2] = c;
            examples[3] = d;
            examples[4] = e;
    }
};

struct MainData {
    Board board;
    Bag bag = Bag(32, 3, 12, 1, 4);
    int x = COLS / 2;
    float y = 0;
    float speed = 0.015;
    float power = 1;
    float score;
    Tile last = Tile(STAR, BLACK);
    int screen = 0, currentLevel = 1;
    int tickCounter = 0;
    bool play = false;

    Message messages[6] = {
        Message("S H U F F L E       P O P\n\nTap or click to continue...",
                Bag(0, 0, 0, 0, 1)),
        Message("Tap or click to select\n"
                "cards as they go past the\n"
                "circle at the bottom of\n"
                "the screen. Match them by\n"
                "color or suite.\n"
                "Get 150 points to continue\n"
                "to the next tutorial level.\n"
                "Tap or click to continue...",
                Bag(1, 0, 0, 0, 0),
                Tile(SUITES[0], COLORS[0]), Tile(SUITES[1], COLORS[1]), Tile(SUITES[2], COLORS[2]), Tile(SUITES[3], COLORS[3]), Tile(SUITES[0], COLORS[2])),
        Message("Star cards can match with\n"
                "any other card.\n"
                "Tap or click to continue...",
                Bag(6, 1, 0, 0, 0),
                Tile(GONE), Tile(GONE), Tile(STAR, BLACK)),
        Message("Movement cards will allow\n"
                "you to move between rows\n"
                "and select cards in\n"
                "different rows.\n"
                "Tap or click to continue...",
                Bag(10, 1, 3, 0, 0),
                Tile(GONE), Tile(MOVES[0]), Tile(GONE), Tile(MOVES[1])),
        Message("Speed cards increase the\n"
                "speed, and are worth 50\n"
                "points.\n"
                "Tap or click to continue...",
                Bag(32, 3, 12, 1, 0),
                Tile(GONE), Tile(GONE), Tile(SPEED)),
        Message("Die cards will shuffle\n"
                "some number of cards above\n"
                "them.\n"
                "You completed the tutorial.\n"
                "Tap or click to continue...",
                Bag(32, 3, 12, 1, 4),
                Tile(DIE + 5), Tile(DIE + 2), Tile(DIE + 4), Tile(DIE), Tile(DIE + 1))
    };

    void fillRow(int row) {
        for (int i = 0; i < COLS; i++) {
            board[row][i] = bag.grab();
        }
    }

    void fillBoard() {
        for (int i = 0; i < ROWS; i++) {
            fillRow(i);
        }
    }

    void init() {
        bag = messages[currentLevel].bag;
        fillBoard();
        x = COLS / 2;
        y = 0;
        speed = 0.015;
        power = 1;
        screen = 0;
        tickCounter = 0;
        last = Tile(STAR, BLACK);
        play = false;
    }

    void mainLoop() {
        tickCounter++;
        BeginDrawing();
        ClearBackground(GREEN);
        if (!play) {
            Rectangle window = {SPACE, GRIDY + SPACE, SCREENWIDTH - 2 * SPACE, SCREENHEIGHT - GRIDY - 2 * SPACE};
            DrawRectangleRounded(window, 0.04, 5, WHITE);
            Message& message = messages[screen];
            for (int i = 0; i < COLS; i++) {
                message.examples[i].draw(i, 0);
            }
            DrawTextEx(font, message.text.c_str(), {2 * SPACE, GRIDY + 2 * SPACE}, FONTSIZE / 3.2, 0, BLACK);
            if (screen == 0 && score != 0) {
                DrawTextEx(font, ("\n\n\nPrevious Score: " + to_string(int(score))).c_str(), {2 * SPACE, GRIDY + 2 * SPACE}, FONTSIZE / 3, 0, BLACK);
            }
            if (tap()) {
                score = 0;
                if (screen == 0 && currentLevel < 5) {
                    screen = currentLevel;
                }
                else {
                    init();
                    play = true;
                }
            }
        }
        else {
            if (floor(y - speed) != floor(y)) {
                fillRow(y - 2);
            }
            y -= speed;
            for (int i = 0; i < ROWS; i++) {
                for (int j = 0; j < COLS; j++) {
                    board[floor(y) + i][j].draw(j, i + floor(y) - y, (j == mod(x, COLS) && i == ROWS - 2));
                }
            }
            if (tap()) {
                int rowSelect = floor(y) + ROWS - 2;
                Tile popped = board[rowSelect][x];
                board[rowSelect][x] = Tile(GONE);
                if (popped.isSuite()) {
                    if (popped.match(last)) {
                        score += BASESCORE + POWERSCORE * power;
                        speed += ACCELERATION * power;
                        power += (1 - power) / 10.0f;
                    }
                    else {
                        power -= 0.1;
                    }
                    last = popped;
                }
                else if (popped.display == '>') {
                    x++;
                }
                else if (popped.display == '<') {
                    x--;
                }
                else if (popped.isSpeed()) {
                    speed += SPEEDBOOST;
                    score += SPEEDSCORE;
                }
                else if (popped.isDie()) {
                    for (int i = 0; i < popped.display - DIE + 2; i++) {
                        board[rowSelect - i][x] = bag.grab();
                    }
                }
            }
            power -= (speed / 50.0f);
            if (score > 150 && currentLevel < 5) {
                currentLevel++;
                screen = currentLevel;
                play = false;
                score = 0;
            }
            if (power < 0) {
                screen = 0;
                play = false;
            }
            last.draw(mod(x, COLS), ROWS - 2.5, false, true);
            if (power > 0.25 || tickCounter / 15 % 2 == 0) {
                DrawRectangle(COLS * GRIDX + SPACE, (1 - power) * (ROWS - 1) * GRIDY, 20, 10000, BLACK);
            }
            else {
                DrawRectangle(COLS * GRIDX + SPACE, (1 - power) * (ROWS - 1) * GRIDY, 20, 10000, RED);
            }
            DrawRectangle(0, (ROWS - 1) * GRIDY + SPACE, COLS * GRIDX + 20 + SPACE, FONTSIZE + SPACE, WHITE);
            DrawTextEx(font, to_string(int(score)).c_str(), {0, (ROWS - 1) * GRIDY + SPACE}, FONTSIZE, 0, BLACK);
        }
        EndDrawing();
    }
};

MainData everything;
void doEverything() {
    everything.mainLoop();
}

int main() {
    InitWindow(SCREENWIDTH, SCREENHEIGHT, "ShufflePop");
    SetTargetFPS(60);
    srand(time(NULL));
    EnableCursor();
    everything.init();

    //Extra characters array (e.g. for dice, suites)
    int extraChars[] = {0x2680, 0x2681, 0x2682, 0x2683, 0x2684, 0x2685, 0x2660, 0x2663, 0x2665, 0x2666, 0x2217};
    //All characters array
    int chars[sizeof(extraChars) / sizeof(int) + 128];
    //Write extended characters into array
    for (int i = 0; i < sizeof(extraChars) / sizeof(int); i++) {
        chars[i] = extraChars[i];
    }
    //Write basic characters into array
    for (int i = 0; i < 128; i++) {
        chars[sizeof(extraChars) /sizeof(int) + i] = i;
    }
    //Load all characters
    font = LoadFontEx("resources/Cousine-Regular-With-Dice.ttf", FONTSIZE, chars, sizeof(chars) / sizeof(int));

    while (!WindowShouldClose()) {
        doEverything();
    }

}
