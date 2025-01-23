#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <termios.h>
#include <unistd.h>
#include <atomic>
#include <poll.h>
#include <csignal>

class Tetris {
private:
    static const int WIDTH = 12;
    static const int HEIGHT = 20;
    std::vector<std::vector<bool>> board;
    std::vector<std::vector<bool>> currentPiece;
    int currentX, currentY;
    std::atomic<bool> gameOver{false};
    bool needRedraw = true;

    const std::vector<std::vector<std::vector<bool>>> shapes = {
        {{true, true},
         {true, true}},
        {{false, true, false},
         {true, true, true}},
        {{true, false},
         {true, false},
         {true, true}},
        {{false, true},
         {false, true},
         {true, true}},
    };

public:
    Tetris() : board(HEIGHT, std::vector<bool>(WIDTH, false)), 
               currentX(WIDTH/2), 
               currentY(0) {
        srand(time(nullptr));
        spawnNewPiece();
    }

    void spawnNewPiece() {
        size_t shapeIndex = rand() % shapes.size();
        currentPiece = shapes[shapeIndex];
        currentX = WIDTH / 2 - currentPiece[0].size() / 2;
        currentY = 0;

        if (isCollision()) {
            gameOver = true;
        }
        needRedraw = true;
    }

    bool isCollision() {
        for (size_t i = 0; i < currentPiece.size(); ++i) {
            for (size_t j = 0; j < currentPiece[i].size(); ++j) {
                if (currentPiece[i][j]) {
                    int newX = currentX + j;
                    int newY = currentY + i;
                    
                    if (newX < 0 || newX >= WIDTH || 
                        newY < 0 || newY >= HEIGHT || 
                        (newY >= 0 && board[newY][newX])) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void rotatePiece() {
        auto rotated = std::vector<std::vector<bool>>(
            currentPiece[0].size(), 
            std::vector<bool>(currentPiece.size(), false)
        );
        
        // 逆时针旋转
        for (size_t i = 0; i < currentPiece.size(); ++i) {
            for (size_t j = 0; j < currentPiece[i].size(); ++j) {
                rotated[currentPiece[0].size() - 1 - j][i] = currentPiece[i][j];
            }
        }
        
        auto oldPiece = currentPiece;
        currentPiece = rotated;
        
        if (isCollision()) {
            // 尝试调整位置
            int originalX = currentX;
            int tries[] = {-1, 1, -2, 2};
            bool canPlace = false;
            
            for (int offset : tries) {
                currentX = originalX + offset;
                if (!isCollision()) {
                    canPlace = true;
                    break;
                }
            }
            
            if (!canPlace) {
                currentPiece = oldPiece;
                currentX = originalX;
            }
        }
        
        needRedraw = true;
    }

    void mergePiece() {
        for (size_t i = 0; i < currentPiece.size(); ++i) {
            for (size_t j = 0; j < currentPiece[i].size(); ++j) {
                if (currentPiece[i][j]) {
                    board[currentY + i][currentX + j] = true;
                }
            }
        }
        needRedraw = true;
    }

    void clearLines() {
        for (int i = HEIGHT - 1; i >= 0; --i) {
            bool lineFull = true;
            for (int j = 0; j < WIDTH; ++j) {
                if (!board[i][j]) {
                    lineFull = false;
                    break;
                }
            }
            if (lineFull) {
                board.erase(board.begin() + i);
                board.insert(board.begin(), std::vector<bool>(WIDTH, false));
                needRedraw = true;
            }
        }
    }

    void moveDown() {
        ++currentY;
        if (isCollision()) {
            --currentY;
            mergePiece();
            clearLines();
            spawnNewPiece();
        } else {
            needRedraw = true;
        }
    }

    void moveLeft() {
        --currentX;
        if (isCollision()) {
            ++currentX;
        } else {
            needRedraw = true;
        }
    }

    void moveRight() {
        ++currentX;
        if (isCollision()) {
            --currentX;
        } else {
            needRedraw = true;
        }
    }

    void draw() {
        if (!needRedraw) return;

        // 移动光标到屏幕顶部
        std::cout << "\033[H";
        
        std::vector<std::vector<bool>> displayBoard = board;
        
        // 绘制当前方块
        for (size_t i = 0; i < currentPiece.size(); ++i) {
            for (size_t j = 0; j < currentPiece[i].size(); ++j) {
                if (currentPiece[i][j]) {
                    int drawX = currentX + j;
                    int drawY = currentY + i;
                    
                    if (drawX >= 0 && drawX < WIDTH && 
                        drawY >= 0 && drawY < HEIGHT) {
                        displayBoard[drawY][drawX] = true;
                    }
                }
            }
        }

        // 绘制游戏区域
        std::cout << "俄罗斯方块游戏\n";
        std::cout << "控制: a左移 d右移 s下移 w旋转 q退出\n";
        std::cout << "┌";
        for (int i = 0; i < WIDTH; ++i) std::cout << "─";
        std::cout << "┐\n";

        for (const auto& row : displayBoard) {
            std::cout << "│";
            for (bool cell : row) {
                std::cout << (cell ? "█" : " ");
            }
            std::cout << "│\n";
        }

        std::cout << "└";
        for (int i = 0; i < WIDTH; ++i) std::cout << "─";
        std::cout << "┘\n";
        std::cout.flush();

        needRedraw = false;
    }

    bool isGameOver() const { return gameOver; }
};

// 设置终端为非规范模式
struct termios orig_termios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    // 重置终端到初始状态
    std::cout << "\033[?25h"; // 显示光标
    std::cout << "\033[0m";   // 重置颜色
    std::cout << "\033[2J";   // 清屏
    std::cout << "\033[H";    // 光标移到左上角
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // 隐藏光标并清屏
    std::cout << "\033[?25l"; // 隐藏光标
    std::cout << "\033[2J";   // 清屏
    std::cout << "\033[H";    // 光标移到左上角
}

int main() {
    enableRawMode();
    
    Tetris game;
    auto lastMoveTime = std::chrono::steady_clock::now();
    
    // 初始绘制
    game.draw();
    
    while (!game.isGameOver()) {
        // 检查是否有输入
        struct pollfd pfds[1];
        pfds[0].fd = STDIN_FILENO;
        pfds[0].events = POLLIN;
        
        if (poll(pfds, 1, 0) > 0) {
            char ch;
            if (read(STDIN_FILENO, &ch, 1) > 0) {
                switch (ch) {
                    case 'a': game.moveLeft(); break;
                    case 'd': game.moveRight(); break;
                    case 's': game.moveDown(); break;
                    case 'w': game.rotatePiece(); break;
                    case 'q': return 0;
                }
                game.draw();
            }
        }
        
        // 自动下落控制
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMoveTime);
        
        if (duration.count() >= 500) {
            game.moveDown();
            game.draw();
            lastMoveTime = now;
        }
    }

    std::cout << "游戏结束！" << std::endl;
    return 0;
}