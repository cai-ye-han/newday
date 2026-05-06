// ================================================
// 打砖块游戏 - 第8课完整版
// 包含：碰撞检测、状态机、道具、粒子、多线程异步加载、网络框架
// ================================================

#include "raylib.h"
#include <vector>
#include <string>
#include <queue>
#include <mutex>
#include <future>
#include <thread>
#include <chrono>
#include <memory>
#include <cstdlib>
#include <ctime>

// ================================================
// 1. 枚举定义
// ================================================
enum class GameState {
    MENU, PLAYING, PAUSED, LOADING, GAMEOVER, VICTORY
};

enum class PowerUpType {
    PADDLE_EXTEND,   // 加长板
    MULTI_BALL,      // 多球
    SLOW_BALL        // 减速球
};

enum class LoadState {
    IDLE, LOADING, DONE
};

// ================================================
// 2. 数据结构
// ================================================
struct Ball {
    Vector2 pos;
    Vector2 speed;
    float radius;
    bool active = true;

    Ball(float x, float y, float r) : pos({ x, y }), speed({ 0, -300 }), radius(r) {}
};

struct Brick {
    Rectangle rect;
    bool active = true;
    Color color;

    Brick(float x, float y, float w, float h)
        : rect({ x, y, w, h }) {
        // 随机颜色
        int r = rand() % 200 + 55;
        int g = rand() % 200 + 55;
        int b = rand() % 200 + 55;
        color = { (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
    }
};

struct Paddle {
    Rectangle rect;
    float speed;
    float originalWidth;
    float extendTimer = 0;
    float extendDuration = 0;
    float extraWidth = 0;

    Paddle(float x, float y, float w, float h, float s)
        : rect({ x, y, w, h }), speed(s), originalWidth(w) {
    }

    void Extend(float extra, float duration) {
        if (extendTimer <= 0) {
            rect.width = originalWidth + extra;
        }
        extraWidth = extra;
        extendDuration = duration;
        extendTimer = duration;
    }

    void Update(float dt) {
        if (extendTimer > 0) {
            extendTimer -= dt;
            if (extendTimer <= 0) {
                rect.width = originalWidth;
            }
        }
    }

    Rectangle GetRect() const { return rect; }
    float GetX() const { return rect.x; }
    float GetSpeed() const { return speed; }
    void SetX(float x) { rect.x = x; }
    void SetWidth(float w) { rect.width = w; originalWidth = w; }
};

struct PowerUp {
    Vector2 pos;
    PowerUpType type;
    bool active = true;
    float speed = 120.0f;
    float timer = 0;  // 旋转动画用

    PowerUp(float x, float y, PowerUpType t) : pos({ x, y }), type(t) {}

    void Update(float dt) {
        pos.y += speed * dt;
        timer += dt;
        if (pos.y > 650) active = false;
    }

    void Draw() {
        // 用不同颜色区分道具类型
        Color c;
        const char* label = "";
        switch (type) {
        case PowerUpType::PADDLE_EXTEND: c = BLUE; label = "W"; break;
        case PowerUpType::MULTI_BALL:    c = GREEN; label = "M"; break;
        case PowerUpType::SLOW_BALL:     c = ORANGE; label = "S"; break;
        }
        // 光晕效果
        DrawCircle(pos.x, pos.y, 12 + sin(timer * 5) * 3, Fade(c, 0.3f));
        DrawCircle(pos.x, pos.y, 8, c);
        DrawText(label, pos.x - 5, pos.y - 7, 12, WHITE);
    }
};

struct Particle {
    Vector2 pos;
    Vector2 vel;
    Color color;
    float life;
    float maxLife;

    Particle(float x, float y, Color c)
        : pos({ x, y }),
        vel({ (float)(rand() % 100 - 50), (float)(rand() % 100 - 50) }),
        color(c), life(0.5f + (rand() % 50) / 100.0f), maxLife(life) {
    }
};

// ================================================
// 3. 线程安全的消息队列（生产者-消费者模式）
// ================================================
template<typename T>
class ThreadSafeQueue {
    std::queue<T> q;
    mutable std::mutex mtx;
public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mtx);
        q.push(value);
    }
    bool pop(T& out) {
        std::lock_guard<std::mutex> lock(mtx);
        if (q.empty()) return false;
        out = q.front();
        q.pop();
        return true;
    }
};

// ================================================
// 4. 关卡数据（用于异步加载）
// ================================================
struct LevelData {
    std::vector<Brick> bricks;
    int levelId;
    Color bgColor;
};

// 模拟耗时加载函数（在后台线程运行）
LevelData LoadLevel(int levelId) {
    // 模拟加载纹理、音效等耗时操作
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    LevelData data;
    data.levelId = levelId;

    // 根据关卡生成不同砖块布局
    int rows = 3 + levelId;
    int cols = 8;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            float x = 10 + j * 98;
            float y = 50 + i * 35;
            data.bricks.emplace_back(x, y, 90, 28);
        }
    }
    data.bgColor = { (unsigned char)(30 + levelId * 30), 20, 40, 255 };
    return data;
}

// ================================================
// 5. 游戏主类
// ================================================
class Game {
public:
    // --- 游戏对象 ---
    std::vector<Ball> balls;
    Paddle paddle;
    std::vector<Brick> bricks;
    std::vector<PowerUp> powerUps;
    std::vector<Particle> particles;

    // --- 游戏状态 ---
    GameState state = GameState::MENU;
    int score = 0;
    int lives = 3;
    int levelId = 1;
    bool allBricksDestroyed = false;

    // --- 异步加载 ---
    LoadState loadState = LoadState::IDLE;
    std::future<LevelData> loadFuture;
    ThreadSafeQueue<std::string> loadedResources;

    // --- 配置参数 ---
    float ballRadius = 10.0f;
    float ballBaseSpeed = 350.0f;
    float ballSlowFactor = 1.0f;
    float slowTimer = 0;

    // --- 构造 ---
    Game() : paddle(350, 550, 120, 18, 400) {
        srand((unsigned int)time(nullptr));
    }

    void Init() {
        // 初始砖块
        SpawnBricks(levelId);
        // 初始球
        balls.emplace_back(400, 300, 10);
    }

    void SpawnBricks(int level) {
        bricks.clear();
        int rows = 3 + level;
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < 8; j++) {
                float x = 10 + j * 98;
                float y = 50 + i * 35;
                bricks.emplace_back(x, y, 90, 28);
            }
        }
    }

    void StartAsyncLoad(int nextLevel) {
        loadState = LoadState::LOADING;
        // 使用 std::async 在后台线程加载
        loadFuture = std::async(std::launch::async, LoadLevel, nextLevel);
        // 演示：用独立线程通过消息队列通知
        std::thread([this, nextLevel]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            loadedResources.push("Level_" + std::to_string(nextLevel) + "_loaded");
            }).detach();
    }

    void Update() {
        float dt = GetFrameTime();

        // --- 菜单输入 ---
        if (state == GameState::MENU) {
            if (IsKeyPressed(KEY_ENTER)) {
                state = GameState::PLAYING;
            }
            return;
        }

        // --- 全局按键 ---
        if (IsKeyPressed(KEY_P) && state == GameState::PLAYING) {
            state = GameState::PAUSED;
        }
        else if (IsKeyPressed(KEY_P) && state == GameState::PAUSED) {
            state = GameState::PLAYING;
        }

        if (state == GameState::GAMEOVER) {
            if (IsKeyPressed(KEY_R)) {
                RestartGame();
            }
            return;
        }

        if (state == GameState::VICTORY) {
            if (IsKeyPressed(KEY_R)) {
                levelId = 1;
                RestartGame();
            }
            return;
        }

        if (state != GameState::PLAYING && state != GameState::LOADING) return;

        // --- 减速计时器 ---
        if (slowTimer > 0) {
            slowTimer -= dt;
            if (slowTimer <= 0) {
                ballSlowFactor = 1.0f;
                for (auto& b : balls) {
                    b.speed.x /= 0.7f;
                    b.speed.y /= 0.7f;
                }
            }
        }

        // --- 板移动 ---
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
            paddle.rect.x -= paddle.speed * dt;
        }
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
            paddle.rect.x += paddle.speed * dt;
        }
        // 边界限制
        if (paddle.rect.x < 0) paddle.rect.x = 0;
        if (paddle.rect.x + paddle.rect.width > 800) paddle.rect.x = 800 - paddle.rect.width;

        paddle.Update(dt);

        // --- 更新球 ---
        for (auto& ball : balls) {
            if (!ball.active) continue;
            ball.pos.x += ball.speed.x * dt;
            ball.pos.y += ball.speed.y * dt;

            // 边界反弹
            if (ball.pos.x - ball.radius < 0) {
                ball.pos.x = ball.radius;
                ball.speed.x = -ball.speed.x;
            }
            if (ball.pos.x + ball.radius > 800) {
                ball.pos.x = 800 - ball.radius;
                ball.speed.x = -ball.speed.x;
            }
            if (ball.pos.y - ball.radius < 0) {
                ball.pos.y = ball.radius;
                ball.speed.y = -ball.speed.y;
            }
        }

        // --- 更新道具 ---
        for (auto& pu : powerUps) {
            pu.Update(dt);
        }

        // --- 更新粒子 ---
        for (auto& p : particles) {
            p.life -= dt;
            p.pos.x += p.vel.x * dt;
            p.pos.y += p.vel.y * dt;
        }
        // 清理死亡粒子和失效道具
        particles.erase(std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p) { return p.life <= 0; }), particles.end());
        powerUps.erase(std::remove_if(powerUps.begin(), powerUps.end(),
            [](const PowerUp& p) { return !p.active; }), powerUps.end());

        // --- 碰撞检测 ---
        CheckAllCollisions();

        // --- 检查球是否全部掉出 ---
        CheckBallLost();

        // --- 检查通关 ---
        allBricksDestroyed = true;
        for (auto& b : bricks) {
            if (b.active) { allBricksDestroyed = false; break; }
        }

        if (allBricksDestroyed && loadState == LoadState::IDLE) {
            state = GameState::LOADING;
            StartAsyncLoad(levelId + 1);
        }

        // --- 检查异步加载完成 ---
        if (loadState == LoadState::LOADING) {
            if (loadFuture.valid() &&
                loadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                LevelData data = loadFuture.get();
                ApplyLevelData(data);
                loadState = LoadState::IDLE;
            }
        }

        // --- 检查消息队列 ---
        std::string msg;
        while (loadedResources.pop(msg)) {
            TraceLog(LOG_INFO, "Async loaded: %s", msg.c_str());
        }

        // --- L键手动触发异步加载演示 ---
        if (IsKeyPressed(KEY_L) && loadState == LoadState::IDLE) {
            state = GameState::LOADING;
            StartAsyncLoad(levelId + 1);
        }
    }

    void CheckAllCollisions() {
        for (auto& ball : balls) {
            if (!ball.active) continue;

            // 球 vs 板
            if (CheckCollisionCircleRec(ball.pos, ball.radius, paddle.GetRect())) {
                ball.speed.y = -fabsf(ball.speed.y);
                // 根据碰撞点偏移调整水平速度
                float offset = ball.pos.x - (paddle.rect.x + paddle.rect.width / 2);
                ball.speed.x += offset * 5.0f;
                // 限制最大速度
                ClampBallSpeed(ball);
            }

            // 球 vs 砖块
            for (auto& brick : bricks) {
                if (!brick.active) continue;
                if (CheckCollisionCircleRec(ball.pos, ball.radius, brick.rect)) {
                    brick.active = false;
                    score += 10;

                    // 反弹
                    ball.speed.y = -ball.speed.y;

                    // 粒子效果
                    SpawnParticles(brick.rect, brick.color);

                    // 30% 概率生成道具
                    if (rand() % 100 < 30) {
                        PowerUpType types[] = {
                            PowerUpType::PADDLE_EXTEND,
                            PowerUpType::MULTI_BALL,
                            PowerUpType::SLOW_BALL
                        };
                        PowerUpType t = types[rand() % 3];
                        powerUps.emplace_back(brick.rect.x + brick.rect.width / 2,
                            brick.rect.y + brick.rect.height / 2, t);
                    }
                    break; // 每帧每个球只处理一个砖块碰撞
                }
            }

            // 道具 vs 板
            for (auto& pu : powerUps) {
                if (!pu.active) continue;
                if (CheckCollisionCircleRec(pu.pos, 10, paddle.GetRect())) {
                    ApplyPowerUp(pu);
                    pu.active = false;
                }
            }
        }
    }

    void SpawnParticles(Rectangle brickRect, Color color) {
        for (int i = 0; i < 15; i++) {
            particles.emplace_back(brickRect.x + rand() % (int)brickRect.width,
                brickRect.y + rand() % (int)brickRect.height,
                color);
        }
    }

    void ApplyPowerUp(const PowerUp& pu) {
        switch (pu.type) {
        case PowerUpType::PADDLE_EXTEND:
            paddle.Extend(50, 5.0f);
            break;
        case PowerUpType::MULTI_BALL: {
            // 生成两个新球
            if (balls.size() < 10) {
                for (int i = 0; i < 2; i++) {
                    Ball newBall(balls[0].pos.x, balls[0].pos.y, 10);
                    newBall.speed.x = (rand() % 200 - 100);
                    newBall.speed.y = -300;
                    balls.push_back(newBall);
                }
            }
            break;
        }
        case PowerUpType::SLOW_BALL:
            if (slowTimer <= 0) {
                for (auto& b : balls) {
                    b.speed.x *= 0.7f;
                    b.speed.y *= 0.7f;
                }
            }
            ballSlowFactor = 0.7f;
            slowTimer = 5.0f;
            break;
        }
    }

    void ClampBallSpeed(Ball& ball) {
        float maxSpeed = 600.0f;
        float spd = sqrtf(ball.speed.x * ball.speed.x + ball.speed.y * ball.speed.y);
        if (spd > maxSpeed) {
            ball.speed.x = ball.speed.x / spd * maxSpeed;
            ball.speed.y = ball.speed.y / spd * maxSpeed;
        }
        float minSpeed = 200.0f;
        if (spd < minSpeed) {
            ball.speed.x = ball.speed.x / spd * minSpeed;
            ball.speed.y = ball.speed.y / spd * minSpeed;
        }
    }

    void CheckBallLost() {
        for (auto& ball : balls) {
            if (ball.pos.y + ball.radius > 620) {
                ball.active = false;
            }
        }
        // 移除出界的球
        balls.erase(std::remove_if(balls.begin(), balls.end(),
            [](const Ball& b) { return !b.active; }), balls.end());

        // 如果没有球了，扣命
        if (balls.empty()) {
            lives--;
            if (lives <= 0) {
                state = GameState::GAMEOVER;
            }
            else {
                balls.emplace_back(400, 300, 10);
                paddle.rect.x = 350;
                paddle.rect.width = paddle.originalWidth;
                ballSlowFactor = 1.0f;
                slowTimer = 0;
            }
        }
    }

    void ApplyLevelData(const LevelData& data) {
        bricks = data.bricks;
        levelId = data.levelId;
        balls.clear();
        balls.emplace_back(400, 300, 10);
        paddle.rect.x = 350;
        loadState = LoadState::IDLE;
        state = GameState::PLAYING;
    }

    void RestartGame() {
        score = 0;
        lives = 3;
        balls.clear();
        balls.emplace_back(400, 300, 10);
        paddle.rect.x = 350;
        paddle.rect.width = paddle.originalWidth;
        ballSlowFactor = 1.0f;
        slowTimer = 0;
        powerUps.clear();
        particles.clear();
        loadState = LoadState::IDLE;
        SpawnBricks(levelId);
        state = GameState::PLAYING;
    }

    void Draw() {
        BeginDrawing();
        ClearBackground({ 20, 20, 40, 255 });

        switch (state) {
        case GameState::MENU:
            DrawMenu();
            break;
        case GameState::PLAYING:
        case GameState::LOADING:
            DrawGame();
            break;
        case GameState::PAUSED:
            DrawGame();
            DrawPauseOverlay();
            break;
        case GameState::GAMEOVER:
            DrawGameOver();
            break;
        case GameState::VICTORY:
            DrawVictory();
            break;
        }

        EndDrawing();
    }

    void DrawMenu() {
        DrawText("BREAKOUT", 230, 180, 60, RAYWHITE);
        DrawText("Press ENTER to Start", 240, 300, 25, GRAY);
        DrawText("Controls: LEFT/RIGHT Arrow or A/D", 190, 360, 20, GRAY);
        DrawText("P = Pause  |  L = Load Next Level", 200, 390, 20, GRAY);
        DrawText("Press L during game to test async loading", 160, 430, 18, DARKGRAY);
    }

    void DrawGame() {
        // 砖块
        for (auto& b : bricks) {
            if (!b.active) continue;
            DrawRectangleRec(b.rect, b.color);
            DrawRectangleLinesEx(b.rect, 1, Fade(WHITE, 0.3f));
        }

        // 粒子
        for (auto& p : particles) {
            float alpha = p.life / p.maxLife;
            DrawCircle(p.pos.x, p.pos.y, 3, Fade(p.color, alpha));
        }

        // 道具
        for (auto& pu : powerUps) {
            pu.Draw();
        }

        // 球
        for (auto& ball : balls) {
            if (!ball.active) continue;
            DrawCircle(ball.pos.x, ball.pos.y, ball.radius, WHITE);
            DrawCircleLines(ball.pos.x, ball.pos.y, ball.radius, GRAY);
        }

        // 板
        DrawRectangleRec(paddle.rect, WHITE);
        // 板光晕（如果加速）
        if (paddle.extendTimer > 0) {
            DrawRectangleRounded(paddle.rect, 0.3f, 8, Fade(BLUE, 0.3f));
        }

        // UI
        DrawText(TextFormat("Score: %d", score), 10, 10, 22, WHITE);
        DrawText(TextFormat("Lives: %d", lives), 680, 10, 22, WHITE);
        DrawText(TextFormat("Level: %d", levelId), 350, 10, 22, WHITE);

        // 减速提示
        if (slowTimer > 0) {
            DrawText("SLOW!", 360, 570, 18, ORANGE);
        }

        // 加载提示
        if (state == GameState::LOADING) {
            DrawRectangle(0, 0, 800, 600, Fade(BLACK, 0.6f));
            DrawText("LOADING NEXT LEVEL...", 200, 270, 35, YELLOW);
            // 旋转动画
            float angle = GetTime() * 360;
            DrawCircleSector({ 400, 350 }, 30, angle, angle + 270, 8, YELLOW);
            DrawText("(Async loading - game not frozen!)", 200, 400, 18, GRAY);
        }
    }

    void DrawPauseOverlay() {
        DrawRectangle(0, 0, 800, 600, Fade(BLACK, 0.5f));
        DrawText("PAUSED", 300, 260, 50, WHITE);
        DrawText("Press P to Resume", 280, 330, 22, GRAY);
    }

    void DrawGameOver() {
        DrawText("GAME OVER", 230, 230, 55, RED);
        DrawText(TextFormat("Final Score: %d", score), 270, 310, 30, WHITE);
        DrawText("Press R to Restart", 270, 370, 22, GRAY);
    }

    void DrawVictory() {
        DrawText("YOU WIN!", 270, 230, 55, GREEN);
        DrawText(TextFormat("Score: %d", score), 300, 310, 30, WHITE);
        DrawText("Press R to Play Again", 260, 370, 22, GRAY);
    }
};

// ================================================
// 6. 主函数
// ================================================
int main() {
    InitWindow(800, 600, "Breakout - Async Loading Demo");
    SetTargetFPS(60);

    Game game;
    game.Init();

    while (!WindowShouldClose()) {
        game.Update();
        game.Draw();
    }

    CloseWindow();
    return 0;
}