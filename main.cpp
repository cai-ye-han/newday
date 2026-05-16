/**
 * 打砖块游戏 - 性能优化版（无 JSON 依赖）
 * 包含：网格法碰撞检测、对象池粒子系统、道具系统、状态机
 * 可在 Visual Studio 中直接编译运行
 *
 * 依赖库：Raylib (需要安装)
 * 下载地址：https://github.com/raysan5/raylib/releases
 */

#include "raylib.h"
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>

// 在包含 raylib 后添加缺失的颜色定义（raylib.h 中未定义 CYAN）
#ifndef CYAN
    #define CYAN CLITERAL(Color){ 0, 255, 255, 255 }
#endif

 // ======================== 常量定义 ========================
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const int TARGET_FPS = 60;

// 网格划分参数（性能优化 - 第十周核心内容）
const int GRID_ROWS = 8;      // 网格行数
const int GRID_COLS = 6;      // 网格列数
const float CELL_WIDTH = SCREEN_WIDTH / (float)GRID_COLS;
const float CELL_HEIGHT = SCREEN_HEIGHT / (float)GRID_ROWS;

// 粒子系统参数（对象池优化）
const int MAX_PARTICLES = 500;

// 游戏配置参数（原 JSON 配置现在硬编码）
struct GameConfig {
    // 球配置
    float ballRadius = 8;
    float ballSpeedX = 180;
    float ballSpeedY = -200;
    float ballMaxSpeed = 400;

    // 挡板配置
    float paddleWidth = 120;
    float paddleHeight = 15;
    float paddleSpeed = 400;

    // 砖块配置
    int bricksRows = 6;
    int bricksCols = 12;
    float brickWidth = 60;
    float brickHeight = 20;
    float brickPadding = 5;
    float bricksOffsetX = 50;
    float bricksOffsetY = 80;

    // 游戏配置
    int initialLives = 3;
    int scorePerBrick = 10;

    // 道具配置
    float powerUpDropRate = 0.3f;
    float paddleExtendWidth = 60;
    float paddleExtendDuration = 5.0f;
    float slowBallFactor = 0.7f;
    float slowBallDuration = 5.0f;
} Config;

// 游戏状态枚举
enum class GameState {
    MENU,
    PLAYING,
    PAUSED,
    GAMEOVER,
    VICTORY
};

// 道具类型枚举
enum class PowerUpType {
    PADDLE_EXTEND,  // 加长挡板
    MULTI_BALL,     // 多球
    SLOW_BALL,      // 减速球
    EXTRA_LIFE      // 额外生命
};

// ======================== 球类 ========================
class Ball {
public:
    Vector2 position;
    Vector2 speed;
    float radius;
    bool active;

    Ball() : radius(Config.ballRadius), active(true) {
        position = { SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f };
        speed = { Config.ballSpeedX, Config.ballSpeedY };
    }

    void Update(float dt) {
        if (!active) return;

        position.x += speed.x * dt;
        position.y += speed.y * dt;

        // 左右边界碰撞
        if (position.x - radius <= 0) {
            position.x = radius;
            speed.x = -speed.x;
        }
        if (position.x + radius >= SCREEN_WIDTH) {
            position.x = SCREEN_WIDTH - radius;
            speed.x = -speed.x;
        }

        // 上边界碰撞
        if (position.y - radius <= 0) {
            position.y = radius;
            speed.y = -speed.y;
        }
    }

    void Draw() const {
        if (active) {
            DrawCircleV(position, radius, WHITE);
            // 绘制拖尾效果（可选）
            DrawCircleLines(position.x, position.y, radius, GRAY);
        }
    }

    void Reset() {
        position = { SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f };
        speed = { Config.ballSpeedX, Config.ballSpeedY };
        active = true;
    }

    Rectangle GetRect() const {
        return { position.x - radius, position.y - radius, radius * 2, radius * 2 };
    }

    void ApplySlow(float factor) {
        speed.x *= factor;
        speed.y *= factor;
        // 限制最小速度
        if (fabs(speed.x) < 50) speed.x = speed.x > 0 ? 50 : -50;
        if (fabs(speed.y) < 50) speed.y = speed.y > 0 ? 50 : -50;
    }
};

// ======================== 挡板类 ========================
class Paddle {
public:
    Rectangle rect;
    float speed;
    Color color;
    float originalWidth;
    float effectRemainingTime;
    bool isSlowed;

    Paddle() : speed(Config.paddleSpeed), color(GREEN), effectRemainingTime(0), isSlowed(false) {
        originalWidth = Config.paddleWidth;
        rect = { SCREEN_WIDTH / 2 - originalWidth / 2,
                SCREEN_HEIGHT - 40,
                originalWidth,
                Config.paddleHeight };
    }

    void Update(float dt) {
        // 更新效果计时
        if (effectRemainingTime > 0) {
            effectRemainingTime -= dt;
            if (effectRemainingTime <= 0) {
                rect.width = originalWidth;
            }
        }

        // 键盘控制
        if (IsKeyDown(KEY_LEFT) && rect.x > 0) {
            rect.x -= speed * dt;
        }
        if (IsKeyDown(KEY_RIGHT) && rect.x + rect.width < SCREEN_WIDTH) {
            rect.x += speed * dt;
        }
    }

    void Draw() const {
        DrawRectangleRec(rect, color);
        // 绘制边框
        DrawRectangleLinesEx(rect, 2, LIGHTGRAY);

        // 如果有道具效果，显示效果指示器
        if (effectRemainingTime > 0) {
            float barWidth = (effectRemainingTime / Config.paddleExtendDuration) * rect.width;
            DrawRectangle(rect.x, rect.y - 5, barWidth, 3, SKYBLUE);
        }
    }

    void Reset() {
        rect.x = SCREEN_WIDTH / 2 - originalWidth / 2;
        rect.width = originalWidth;
        effectRemainingTime = 0;
    }

    void Extend(float extraWidth, float duration) {
        rect.width = originalWidth + extraWidth;
        // 防止超出屏幕
        if (rect.x + rect.width > SCREEN_WIDTH) {
            rect.x = SCREEN_WIDTH - rect.width;
        }
        effectRemainingTime = duration;
    }

    Rectangle GetRect() const { return rect; }
};

// ======================== 砖块类 ========================
class Brick {
public:
    Rectangle rect;
    Color color;
    bool active;
    int hp;  // 部分砖块可能需要多次击打

    Brick() : active(false), hp(1) {}

    Brick(float x, float y, float w, float h, Color col, int hitPoints = 1)
        : active(true), color(col), hp(hitPoints) {
        rect = { x, y, w, h };
    }

    void Draw() const {
        if (active) {
            DrawRectangleRec(rect, color);
            DrawRectangleLinesEx(rect, 1, DARKGRAY);

            // 显示血量（如果大于1）
            if (hp > 1) {
                DrawText(TextFormat("%d", hp),
                    rect.x + rect.width / 2 - 5,
                    rect.y + rect.height / 2 - 8,
                    15, WHITE);
            }
        }
    }

    bool Hit() {
        hp--;
        if (hp <= 0) {
            active = false;
            return true;  // 砖块被摧毁
        }
        // 改变颜色表示受伤
        color.a = 150;
        return false;  // 砖块未被摧毁
    }
};

// ======================== 粒子系统（对象池优化 - 第十周核心内容）=======================
struct Particle {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float life;
    bool active;
};

class ParticleSystem {
private:
    Particle particles[MAX_PARTICLES];

public:
    ParticleSystem() {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            particles[i].active = false;
        }
    }

    void Spawn(Vector2 pos, Vector2 vel, Color color, float life) {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (!particles[i].active) {
                particles[i].position = pos;
                particles[i].velocity = vel;
                particles[i].color = color;
                particles[i].life = life;
                particles[i].active = true;
                return;
            }
        }
    }

    void SpawnExplosion(Vector2 pos, Color color) {
        for (int i = 0; i < 12; i++) {
            Vector2 vel = {
                (rand() % 200 - 100) / 100.0f,
                (rand() % 200 - 100) / 100.0f
            };
            Spawn(pos, vel, color, 0.6f);
        }
    }

    void SpawnTrail(Vector2 pos, Color color) {
        Vector2 vel = { (rand() % 100 - 50) / 100.0f, (rand() % 100 - 50) / 100.0f };
        Spawn(pos, vel, color, 0.3f);
    }

    void Update(float dt) {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].active) {
                particles[i].position.x += particles[i].velocity.x * dt * 200;
                particles[i].position.y += particles[i].velocity.y * dt * 200;
                particles[i].life -= dt;

                // 重力效果
                particles[i].velocity.y += dt * 100;

                if (particles[i].life <= 0) {
                    particles[i].active = false;
                }
            }
        }
    }

    void Draw() const {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].active) {
                // 根据生命值改变透明度
                Color color = particles[i].color;
                color.a = (unsigned char)(255 * (particles[i].life / 0.6f));
                DrawCircleV(particles[i].position, 3, color);
            }
        }
    }

    void Clear() {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            particles[i].active = false;
        }
    }
};

// ======================== 道具类 ========================
class PowerUp {
public:
    Vector2 position;
    PowerUpType type;
    bool active;
    float speed;
    float rotation;

    PowerUp() : active(false), speed(100), rotation(0) {}

    PowerUp(float x, float y, PowerUpType t) : active(true), speed(100), rotation(0) {
        position = { x, y };
        type = t;
    }

    void Update(float dt) {
        if (active) {
            position.y += speed * dt;
            rotation += dt * 180;  // 旋转效果
        }
    }

    void Draw() const {
        if (!active) return;

        Color color;
        switch (type) {
        case PowerUpType::PADDLE_EXTEND: color = BLUE; break;
        case PowerUpType::MULTI_BALL: color = ORANGE; break;
        case PowerUpType::SLOW_BALL: color = PURPLE; break;
        case PowerUpType::EXTRA_LIFE: color = RED; break;
        }

        // 绘制发光效果
        DrawCircleGradient(position.x, position.y, 15, Fade(color, 0.3f), Fade(color, 0.8f));
        DrawCircleV(position, 10, color);
        DrawCircleLines(position.x, position.y, 10, WHITE);

        // 绘制旋转的星形效果
        float rad = rotation * DEG2RAD;
        for (int i = 0; i < 4; i++) {
            float angle = rad + i * PI / 2;
            float x = position.x + cos(angle) * 12;
            float y = position.y + sin(angle) * 12;
            DrawCircleV({ x, y }, 2, WHITE);
        }
    }

    Rectangle GetRect() const {
        return { position.x - 10, position.y - 10, 20, 20 };
    }
};

// ======================== 游戏主类 ========================
class Game {
private:
    // 游戏对象
    std::vector<Ball> balls;
    Paddle paddle;
    std::vector<Brick> bricks;
    std::vector<PowerUp> powerUps;
    ParticleSystem particles;

    // 游戏状态
    GameState state;
    int lives;
    int score;
    bool gameRunning;
    float slowEffectRemaining;  // 减速效果剩余时间

    // 网格（性能优化 - 第十周核心内容：空间划分）
    std::vector<Brick*> grid[GRID_COLS][GRID_ROWS];

    // 性能统计
    float collisionTime;
    float particleTime;
    float frameTime;

public:
    Game() : state(GameState::MENU), lives(Config.initialLives), score(0),
        gameRunning(true), slowEffectRemaining(0),
        collisionTime(0), particleTime(0), frameTime(0) {
        srand((unsigned int)time(nullptr));
        InitGame();
    }

    void InitGame() {
        // 初始化球
        balls.clear();
        balls.push_back(Ball());

        // 初始化挡板
        paddle.Reset();

        // 初始化砖块
        bricks.clear();
        InitBricks();

        // 清空道具
        powerUps.clear();

        // 清除粒子
        particles.Clear();

        // 重置分数和生命
        score = 0;
        lives = Config.initialLives;
        slowEffectRemaining = 0;
        state = GameState::PLAYING;
    }

    void InitBricks() {
        float startX = Config.bricksOffsetX;
        float startY = Config.bricksOffsetY;

        // 计算实际砖块宽度以填满屏幕
        float totalWidth = (Config.bricksCols * (Config.brickWidth + Config.brickPadding));
        startX = (SCREEN_WIDTH - totalWidth) / 2;

        for (int row = 0; row < Config.bricksRows; row++) {
            // 根据行数设置不同颜色和血量
            Color color;
            int hp = 1;

            switch (row % 5) {
            case 0: color = RED; hp = 1; break;
            case 1: color = ORANGE; hp = 1; break;
            case 2: color = YELLOW; hp = 1; break;
            case 3: color = GREEN; hp = 2; break;      // 需要打两次
            case 4: color = SKYBLUE; hp = 3; break;    // 需要打三次
            default: color = PURPLE; hp = 1; break;
            }

            for (int col = 0; col < Config.bricksCols; col++) {
                float x = startX + col * (Config.brickWidth + Config.brickPadding);
                float y = startY + row * (Config.brickHeight + Config.brickPadding);
                bricks.push_back(Brick(x, y, Config.brickWidth, Config.brickHeight, color, hp));
            }
        }
    }

    void Update(float dt) {
        frameTime = dt;
        double frameStart = GetTime();

        switch (state) {
        case GameState::MENU:
            UpdateMenu();
            break;
        case GameState::PLAYING:
            UpdatePlaying(dt);
            break;
        case GameState::PAUSED:
            UpdatePaused();
            break;
        case GameState::GAMEOVER:
        case GameState::VICTORY:
            UpdateGameOver();
            break;
        }

        double frameEnd = GetTime();
        // 性能统计
    }

    void UpdateMenu() {
        if (IsKeyPressed(KEY_ENTER)) {
            InitGame();
            state = GameState::PLAYING;
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            gameRunning = false;
        }
    }

    void UpdatePlaying(float dt) {
        // 更新减速效果
        if (slowEffectRemaining > 0) {
            slowEffectRemaining -= dt;
            if (slowEffectRemaining <= 0) {
                // 恢复速度
                for (auto& ball : balls) {
                    ball.speed.x /= Config.slowBallFactor;
                    ball.speed.y /= Config.slowBallFactor;
                }
            }
        }

        // 更新挡板
        paddle.Update(dt);

        // 更新所有球
        for (auto& ball : balls) {
            ball.Update(dt);
            // 添加拖尾粒子
            if (ball.active && (rand() % 10 < 3)) {
                particles.SpawnTrail(ball.position, WHITE);
            }
        }

        // 更新道具
        for (auto& powerUp : powerUps) {
            powerUp.Update(dt);
        }

        // 更新粒子系统
        double particleStart = GetTime();
        particles.Update(dt);
        particleTime = (float)(GetTime() - particleStart);

        // 碰撞检测（使用网格优化）
        double collisionStart = GetTime();
        CheckCollisions();
        collisionTime = (float)(GetTime() - collisionStart);

        // 检查球是否掉出屏幕
        CheckBallsOut();

        // 检查胜利条件
        CheckVictory();

        // 清理失效的道具
        powerUps.erase(
            std::remove_if(powerUps.begin(), powerUps.end(),
                [](const PowerUp& p) {
                    return !p.active || p.position.y > SCREEN_HEIGHT + 50;
                }),
            powerUps.end()
        );

        // 清理失效的球
        balls.erase(
            std::remove_if(balls.begin(), balls.end(),
                [](const Ball& b) { return !b.active; }),
            balls.end()
        );

        // 如果没有球了，减少生命并重置
        if (balls.empty()) {
            lives--;
            if (lives <= 0) {
                state = GameState::GAMEOVER;
            }
            else {
                balls.push_back(Ball());
                paddle.Reset();
            }
        }

        // 暂停
        if (IsKeyPressed(KEY_P)) {
            state = GameState::PAUSED;
        }
    }

    void UpdatePaused() {
        if (IsKeyPressed(KEY_P)) {
            state = GameState::PLAYING;
        }
    }

    void UpdateGameOver() {
        if (IsKeyPressed(KEY_ENTER)) {
            InitGame();
            state = GameState::PLAYING;
        }
    }

    void CheckCollisions() {
        // 更新网格（空间划分）
        UpdateSpatialGrid();

        // 球与挡板碰撞
        for (auto& ball : balls) {
            if (CheckCollisionCircleRec(ball.position, ball.radius, paddle.GetRect())) {
                // 确保球向上反弹
                ball.speed.y = -fabs(ball.speed.y);

                // 根据碰撞点偏移改变水平速度（增加游戏趣味）
                float offset = ball.position.x - (paddle.rect.x + paddle.rect.width / 2);
                ball.speed.x += offset * 5.0f;

                // 限制最大速度
                if (ball.speed.x > Config.ballMaxSpeed) ball.speed.x = Config.ballMaxSpeed;
                if (ball.speed.x < -Config.ballMaxSpeed) ball.speed.x = -Config.ballMaxSpeed;
                if (ball.speed.y > Config.ballMaxSpeed) ball.speed.y = Config.ballMaxSpeed;

                // 粒子特效
                particles.SpawnExplosion(ball.position, YELLOW);
            }
        }

        // 球与砖块碰撞（使用网格优化 - 性能关键）
        for (auto& ball : balls) {
            int gridX = (int)(ball.position.x / CELL_WIDTH);
            int gridY = (int)(ball.position.y / CELL_HEIGHT);
            gridX = std::clamp(gridX, 0, GRID_COLS - 1);
            gridY = std::clamp(gridY, 0, GRID_ROWS - 1);

            // 只遍历相邻网格（3x3区域）
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    int nx = gridX + dx;
                    int ny = gridY + dy;
                    if (nx >= 0 && nx < GRID_COLS && ny >= 0 && ny < GRID_ROWS) {
                        for (auto* brick : grid[nx][ny]) {
                            if (brick && brick->active &&
                                CheckCollisionCircleRec(ball.position, ball.radius, brick->rect)) {

                                bool destroyed = brick->Hit();

                                if (destroyed) {
                                    score += Config.scorePerBrick;

                                    // 粒子特效
                                    particles.SpawnExplosion(
                                        { brick->rect.x + brick->rect.width / 2,
                                         brick->rect.y + brick->rect.height / 2 },
                                        brick->color
                                    );

                                    // 随机生成道具
                                    if ((rand() % 100) / 100.0f < Config.powerUpDropRate) {
                                        PowerUpType type = static_cast<PowerUpType>(rand() % 4);
                                        powerUps.push_back(PowerUp(
                                            brick->rect.x + brick->rect.width / 2,
                                            brick->rect.y + brick->rect.height / 2,
                                            type
                                        ));
                                    }
                                }

                                // 球反弹（根据碰撞位置决定方向）
                                float overlapLeft = (ball.position.x + ball.radius) - brick->rect.x;
                                float overlapRight = (brick->rect.x + brick->rect.width) - (ball.position.x - ball.radius);
                                float overlapTop = (ball.position.y + ball.radius) - brick->rect.y;
                                float overlapBottom = (brick->rect.y + brick->rect.height) - (ball.position.y - ball.radius);

                                // 选择最小的重叠方向反弹
                                float minOverlap = std::min({ overlapLeft, overlapRight, overlapTop, overlapBottom });
                                if (minOverlap == overlapLeft || minOverlap == overlapRight) {
                                    ball.speed.x = -ball.speed.x;
                                }
                                else {
                                    ball.speed.y = -ball.speed.y;
                                }

                                goto next_ball; // 一次只处理一个碰撞
                            }
                        }
                    }
                }
            }
        next_ball:;
        }

        // 道具与挡板碰撞
        for (auto& powerUp : powerUps) {
            if (powerUp.active && CheckCollisionRecs(paddle.GetRect(), powerUp.GetRect())) {
                ApplyPowerUp(powerUp.type);
                powerUp.active = false;
                particles.SpawnExplosion(powerUp.position, CYAN);
            }
        }
    }

    void UpdateSpatialGrid() {
        // 清空网格
        for (int i = 0; i < GRID_COLS; i++) {
            for (int j = 0; j < GRID_ROWS; j++) {
                grid[i][j].clear();
            }
        }

        // 将活跃的砖块放入网格
        for (auto& brick : bricks) {
            if (brick.active) {
                int gridX = (int)(brick.rect.x / CELL_WIDTH);
                int gridY = (int)(brick.rect.y / CELL_HEIGHT);
                gridX = std::clamp(gridX, 0, GRID_COLS - 1);
                gridY = std::clamp(gridY, 0, GRID_ROWS - 1);
                grid[gridX][gridY].push_back(&brick);
            }
        }
    }

    void ApplyPowerUp(PowerUpType type) {
        switch (type) {
        case PowerUpType::PADDLE_EXTEND:
            paddle.Extend(Config.paddleExtendWidth, Config.paddleExtendDuration);
            break;
        case PowerUpType::MULTI_BALL:
            if (balls.size() < 5) {
                Ball newBall;
                newBall.position = balls[0].position;
                newBall.speed = { balls[0].speed.x + 80, balls[0].speed.y + 50 };
                balls.push_back(newBall);

                newBall = Ball();
                newBall.position = balls[0].position;
                newBall.speed = { balls[0].speed.x - 80, balls[0].speed.y + 50 };
                balls.push_back(newBall);
            }
            break;
        case PowerUpType::SLOW_BALL:
            if (slowEffectRemaining <= 0) {
                for (auto& ball : balls) {
                    ball.ApplySlow(Config.slowBallFactor);
                }
            }
            slowEffectRemaining = Config.slowBallDuration;
            break;
        case PowerUpType::EXTRA_LIFE:
            lives++;
            break;
        }
    }

    void CheckBallsOut() {
        for (auto& ball : balls) {
            if (ball.position.y + ball.radius > SCREEN_HEIGHT) {
                ball.active = false;
                particles.SpawnExplosion(ball.position, RED);
            }
        }
    }

    void CheckVictory() {
        bool allDestroyed = true;
        for (const auto& brick : bricks) {
            if (brick.active) {
                allDestroyed = false;
                break;
            }
        }
        if (allDestroyed) {
            state = GameState::VICTORY;
        }
    }

    void Draw() {
        BeginDrawing();
        ClearBackground(BLACK);

        switch (state) {
        case GameState::MENU:
            DrawMenu();
            break;
        case GameState::PLAYING:
        case GameState::PAUSED:
            DrawGame();
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

    void DrawGame() {
        // 绘制砖块
        for (const auto& brick : bricks) {
            brick.Draw();
        }

        // 绘制挡板
        paddle.Draw();

        // 绘制所有球
        for (const auto& ball : balls) {
            ball.Draw();
        }

        // 绘制道具
        for (const auto& powerUp : powerUps) {
            powerUp.Draw();
        }

        // 绘制粒子
        particles.Draw();

        // 绘制UI
        DrawUI();

        // 暂停画面
        if (state == GameState::PAUSED) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.7f));
            DrawText("PAUSED", SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2, 40, WHITE);
            DrawText("Press P to resume", SCREEN_WIDTH / 2 - 110, SCREEN_HEIGHT / 2 + 50, 20, WHITE);
        }
    }

    void DrawUI() {
        // 分数
        DrawText(TextFormat("Score: %d", score), 10, 10, 25, WHITE);

        // 生命值（用红心表示）
        for (int i = 0; i < lives; i++) {
            DrawCircle(30 + i * 35, 50, 12, RED);
            DrawText("❤", 27 + i * 35, 43, 15, WHITE);
        }

        // 性能信息（帧率 + 碰撞检测耗时）
        DrawText(TextFormat("FPS: %d", GetFPS()), SCREEN_WIDTH - 100, 10, 15, GREEN);
        DrawText(TextFormat("Collision: %.2f ms", collisionTime * 1000),
            SCREEN_WIDTH - 130, 30, 12, YELLOW);

        // 砖块剩余数量
        int remaining = 0;
        for (const auto& brick : bricks) {
            if (brick.active) remaining++;
        }
        DrawText(TextFormat("Bricks: %d", remaining), SCREEN_WIDTH - 100, 50, 12, ORANGE);

        // 道具效果提示
        if (slowEffectRemaining > 0) {
            DrawText(TextFormat("SLOW: %.1f", slowEffectRemaining),
                SCREEN_WIDTH / 2 - 50, 10, 15, PURPLE);
        }
        if (paddle.effectRemainingTime > 0) {
            DrawText(TextFormat("EXTEND: %.1f", paddle.effectRemainingTime),
                SCREEN_WIDTH / 2 - 60, 30, 15, BLUE);
        }
    }

    void DrawMenu() {
        const char* title = "BREAKOUT";
        const char* subtitle = "Performance Optimized Edition";
        const char* start = "Press ENTER to Start";
        const char* quit = "Press ESC to Quit";

        int titleWidth = MeasureText(title, 70);
        int subtitleWidth = MeasureText(subtitle, 20);
        int startWidth = MeasureText(start, 25);
        int quitWidth = MeasureText(quit, 20);

        // 标题动画效果
        float time = GetTime();
        Color titleColor = { 255, (unsigned char)(128 + sin(time) * 127), 0, 255 };

        DrawText(title, SCREEN_WIDTH / 2 - titleWidth / 2, SCREEN_HEIGHT / 3 - 30, 70, titleColor);
        DrawText(subtitle, SCREEN_WIDTH / 2 - subtitleWidth / 2, SCREEN_HEIGHT / 3 + 40, 20, GRAY);
        DrawText(start, SCREEN_WIDTH / 2 - startWidth / 2, SCREEN_HEIGHT / 2, 25, YELLOW);
        DrawText(quit, SCREEN_WIDTH / 2 - quitWidth / 2, SCREEN_HEIGHT / 2 + 50, 20, GRAY);

        // 操作说明
        DrawText("CONTROLS:", 50, SCREEN_HEIGHT - 130, 20, WHITE);
        DrawText("LEFT/RIGHT  - Move paddle", 50, SCREEN_HEIGHT - 105, 15, GRAY);
        DrawText("P           - Pause game", 50, SCREEN_HEIGHT - 85, 15, GRAY);
        DrawText("ESC         - Quit", 50, SCREEN_HEIGHT - 65, 15, GRAY);

        // 优化特性说明
        DrawText("OPTIMIZATIONS:", SCREEN_WIDTH - 200, SCREEN_HEIGHT - 130, 15, WHITE);
        DrawText("Spatial Grid (8x6)", SCREEN_WIDTH - 200, SCREEN_HEIGHT - 110, 12, GREEN);
        DrawText("Particle Object Pool", SCREEN_WIDTH - 200, SCREEN_HEIGHT - 95, 12, GREEN);
        DrawText("State Machine", SCREEN_WIDTH - 200, SCREEN_HEIGHT - 80, 12, GREEN);
        DrawText("Power-Up System", SCREEN_WIDTH - 200, SCREEN_HEIGHT - 65, 12, GREEN);
    }

    void DrawGameOver() {
        DrawGame();

        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));

        const char* text = "GAME OVER";
        const char* restart = "Press ENTER to Restart";

        int textWidth = MeasureText(text, 55);
        int restartWidth = MeasureText(restart, 25);

        DrawText(text, SCREEN_WIDTH / 2 - textWidth / 2, SCREEN_HEIGHT / 3, 55, RED);
        DrawText(restart, SCREEN_WIDTH / 2 - restartWidth / 2, SCREEN_HEIGHT / 2, 25, WHITE);
        DrawText(TextFormat("Final Score: %d", score),
            SCREEN_WIDTH / 2 - MeasureText(TextFormat("Final Score: %d", score), 30) / 2,
            SCREEN_HEIGHT / 2 + 50, 30, YELLOW);
    }

    void DrawVictory() {
        DrawGame();

        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));

        const char* text = "VICTORY!";
        const char* restart = "Press ENTER to Play Again";

        int textWidth = MeasureText(text, 65);
        int restartWidth = MeasureText(restart, 25);

        // 胜利动画
        float time = GetTime();
        Color victoryColor = { 0, (unsigned char)(128 + sin(time) * 127), 0, 255 };

        DrawText(text, SCREEN_WIDTH / 2 - textWidth / 2, SCREEN_HEIGHT / 3, 65, victoryColor);
        DrawText(restart, SCREEN_WIDTH / 2 - restartWidth / 2, SCREEN_HEIGHT / 2, 25, WHITE);
        DrawText(TextFormat("Final Score: %d", score),
            SCREEN_WIDTH / 2 - MeasureText(TextFormat("Final Score: %d", score), 30) / 2,
            SCREEN_HEIGHT / 2 + 50, 30, YELLOW);
    }

    bool IsRunning() const { return gameRunning; }
};

// ======================== 主函数 ========================
int main() {
    // 初始化窗口
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Breakout - Performance Optimized");
    SetTargetFPS(TARGET_FPS);
    InitAudioDevice();  // 初始化音频设备（可选）

    // 设置随机种子
    srand((unsigned int)time(nullptr));

    // 创建游戏实例
    Game game;

    // 主循环
    while (!WindowShouldClose() && game.IsRunning()) {
        float dt = GetFrameTime();
        // 限制最大 delta time，防止物理跳跃过大
        if (dt > 0.033f) dt = 0.033f;

        game.Update(dt);
        game.Draw();
    }

    // 清理
    CloseAudioDevice();
    CloseWindow();

    return 0;
}