/**
 * 打砖块游戏 - 数据持久化与关卡编辑器完整实现
 * 包含：JSON配置驱动、存档/读档、多关卡系统、编辑模式
 *
 * 编译命令（需要安装raylib和nlohmann/json）：
 * g++ -o breakout main.cpp -lraylib -lwinmm -lgdi32 -std=c++17
 *
 * 或在 Visual Studio 中：
 * 1. 安装 vcpkg: vcpkg install raylib nlohmann-json
 * 2. 项目属性中链接 raylib.lib 和 nlohmann_json.lib
 */

#include "raylib.h"
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <direct.h>  // for _mkdir

 // ======================== JSON 简化实现（不依赖外部库）========================
 // 简单的JSON解析器（支持基本的JSON格式）
class SimpleJSON {
private:
    std::map<std::string, std::string> stringValues;
    std::map<std::string, int> intValues;
    std::map<std::string, float> floatValues;
    std::map<std::string, bool> boolValues;
    std::map<std::string, std::vector<std::vector<int>>> array2DValues;

public:
    bool Parse(const std::string& content) {
        size_t pos = 0;
        return ParseObject(content, pos);
    }

    bool ParseObject(const std::string& json, size_t& pos) {
        SkipWhitespace(json, pos);
        if (json[pos] != '{') return false;
        pos++;

        while (pos < json.length()) {
            SkipWhitespace(json, pos);
            if (json[pos] == '}') {
                pos++;
                break;
            }

            // 解析键
            std::string key = ParseString(json, pos);
            SkipWhitespace(json, pos);
            if (json[pos] != ':') return false;
            pos++;
            SkipWhitespace(json, pos);

            // 解析值
            if (json[pos] == '{') {
                // 嵌套对象，递归解析
                ParseObject(json, pos);
            }
            else if (json[pos] == '[') {
                // 解析数组
                ParseArray(json, pos, key);
            }
            else if (json[pos] == '"') {
                std::string value = ParseString(json, pos);
                stringValues[key] = value;
            }
            else if (json[pos] >= '0' && json[pos] <= '9' || json[pos] == '-') {
                // 数字
                std::string numStr;
                while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-')) {
                    numStr += json[pos];
                    pos++;
                }
                if (numStr.find('.') != std::string::npos) {
                    floatValues[key] = std::stof(numStr);
                }
                else {
                    intValues[key] = std::stoi(numStr);
                }
                continue;
            }
            else if (json[pos] == 't' || json[pos] == 'f') {
                // 布尔值
                if (json.substr(pos, 4) == "true") {
                    boolValues[key] = true;
                    pos += 4;
                }
                else if (json.substr(pos, 5) == "false") {
                    boolValues[key] = false;
                    pos += 5;
                }
                continue;
            }

            SkipWhitespace(json, pos);
            if (json[pos] == ',') {
                pos++;
            }
        }
        return true;
    }

    void ParseArray(const std::string& json, size_t& pos, const std::string& key) {
        pos++; // 跳过 '['
        std::vector<std::vector<int>> array;

        while (pos < json.length()) {
            SkipWhitespace(json, pos);
            if (json[pos] == ']') {
                pos++;
                break;
            }

            if (json[pos] == '[') {
                // 二维数组
                pos++;
                std::vector<int> row;
                while (pos < json.length()) {
                    SkipWhitespace(json, pos);
                    if (json[pos] == ']') {
                        pos++;
                        break;
                    }
                    if (json[pos] >= '0' && json[pos] <= '9') {
                        int val = 0;
                        while (pos < json.length() && isdigit(json[pos])) {
                            val = val * 10 + (json[pos] - '0');
                            pos++;
                        }
                        row.push_back(val);
                    }
                    SkipWhitespace(json, pos);
                    if (json[pos] == ',') pos++;
                }
                array.push_back(row);
            }

            SkipWhitespace(json, pos);
            if (json[pos] == ',') pos++;
        }
        array2DValues[key] = array;
    }

    std::string ParseString(const std::string& json, size_t& pos) {
        if (json[pos] != '"') return "";
        pos++;
        std::string result;
        while (pos < json.length() && json[pos] != '"') {
            if (json[pos] == '\\') {
                pos++;
                if (json[pos] == 'n') result += '\n';
                else if (json[pos] == 't') result += '\t';
                else result += json[pos];
            }
            else {
                result += json[pos];
            }
            pos++;
        }
        pos++;
        return result;
    }

    void SkipWhitespace(const std::string& str, size_t& pos) {
        while (pos < str.length() && (str[pos] == ' ' || str[pos] == '\n' || str[pos] == '\r' || str[pos] == '\t')) {
            pos++;
        }
    }

    int GetInt(const std::string& key, int defaultValue = 0) const {
        auto it = intValues.find(key);
        return it != intValues.end() ? it->second : defaultValue;
    }

    float GetFloat(const std::string& key, float defaultValue = 0.0f) const {
        auto it = floatValues.find(key);
        return it != floatValues.end() ? it->second : defaultValue;
    }

    std::string GetString(const std::string& key, const std::string& defaultValue = "") const {
        auto it = stringValues.find(key);
        return it != stringValues.end() ? it->second : defaultValue;
    }

    bool GetBool(const std::string& key, bool defaultValue = false) const {
        auto it = boolValues.find(key);
        return it != boolValues.end() ? it->second : defaultValue;
    }

    std::vector<std::vector<int>> GetArray2D(const std::string& key) const {
        auto it = array2DValues.find(key);
        return it != array2DValues.end() ? it->second : std::vector<std::vector<int>>();
    }

    bool HasKey(const std::string& key) const {
        return intValues.find(key) != intValues.end() ||
            floatValues.find(key) != floatValues.end() ||
            stringValues.find(key) != stringValues.end() ||
            boolValues.find(key) != boolValues.end() ||
            array2DValues.find(key) != array2DValues.end();
    }
};

// ======================== 颜色定义 ========================
#ifndef CYAN
#define CYAN CLITERAL(Color){ 0, 255, 255, 255 }
#endif

// ======================== 常量定义 ========================
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const int TARGET_FPS = 60;

// 游戏配置（从JSON加载）
struct GameConfig {
    float ballRadius = 8.0f;
    float ballSpeedX = 180.0f;
    float ballSpeedY = -200.0f;
    float ballMaxSpeed = 400.0f;

    float paddleWidth = 120.0f;
    float paddleHeight = 15.0f;
    float paddleSpeed = 400.0f;

    int initialLives = 3;
    int scorePerBrick = 10;
    float powerUpDropRate = 0.3f;

    float paddleExtendWidth = 60.0f;
    float paddleExtendDuration = 5.0f;
    float slowBallFactor = 0.7f;
    float slowBallDuration = 5.0f;

    int totalLevels = 5;
} Config;

// 游戏状态枚举
enum class GameState {
    MENU,
    PLAYING,
    PAUSED,
    GAMEOVER,
    VICTORY,
    EDIT_MODE
};

// 道具类型枚举
enum class PowerUpType {
    PADDLE_EXTEND,
    MULTI_BALL,
    SLOW_BALL,
    EXTRA_LIFE
};

// 存档数据结构
struct SaveData {
    int version = 1;
    int currentLevel = 1;
    int score = 0;
    int lives = 3;
    float slowEffectRemaining = 0.0f;
    float paddleEffectRemaining = 0.0f;
    time_t saveTime = 0;
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

        if (position.x - radius <= 0) {
            position.x = radius;
            speed.x = -speed.x;
        }
        if (position.x + radius >= SCREEN_WIDTH) {
            position.x = SCREEN_WIDTH - radius;
            speed.x = -speed.x;
        }
        if (position.y - radius <= 0) {
            position.y = radius;
            speed.y = -speed.y;
        }
    }

    void Draw() const {
        if (active) {
            DrawCircleV(position, radius, WHITE);
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

    Paddle() : speed(Config.paddleSpeed), color(GREEN), effectRemainingTime(0) {
        originalWidth = Config.paddleWidth;
        rect = { SCREEN_WIDTH / 2 - originalWidth / 2,
                SCREEN_HEIGHT - 40,
                originalWidth,
                Config.paddleHeight };
    }

    void Update(float dt) {
        if (effectRemainingTime > 0) {
            effectRemainingTime -= dt;
            if (effectRemainingTime <= 0) {
                rect.width = originalWidth;
            }
        }

        if (IsKeyDown(KEY_LEFT) && rect.x > 0) {
            rect.x -= speed * dt;
        }
        if (IsKeyDown(KEY_RIGHT) && rect.x + rect.width < SCREEN_WIDTH) {
            rect.x += speed * dt;
        }
    }

    void Draw() const {
        DrawRectangleRec(rect, color);
        DrawRectangleLinesEx(rect, 2, LIGHTGRAY);

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
    int hp;
    int scoreValue;

    Brick() : active(false), hp(1), scoreValue(10) {}

    Brick(float x, float y, float w, float h, Color col, int hitPoints = 1, int score = 10)
        : active(true), color(col), hp(hitPoints), scoreValue(score) {
        rect = { x, y, w, h };
    }

    void Draw() const {
        if (active) {
            DrawRectangleRec(rect, color);
            DrawRectangleLinesEx(rect, 1, DARKGRAY);
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
            return true;
        }
        color.a = 150;
        return false;
    }
};

// ======================== 粒子系统 ========================
struct Particle {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float life;
    bool active;
};

class ParticleSystem {
private:
    static const int MAX_PARTICLES = 500;
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
            rotation += dt * 180;
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

        DrawCircleGradient(position.x, position.y, 15, Fade(color, 0.3f), Fade(color, 0.8f));
        DrawCircleV(position, 10, color);
        DrawCircleLines(position.x, position.y, 10, WHITE);

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

// ======================== 配置加载器 ========================
class ConfigLoader {
public:
    static bool LoadConfig(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cout << "配置文件不存在，使用默认配置" << std::endl;
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        SimpleJSON json;
        if (!json.Parse(buffer.str())) {
            std::cout << "JSON解析失败，使用默认配置" << std::endl;
            return false;
        }

        // 加载配置
        Config.ballRadius = json.GetFloat("ball_radius", 8.0f);
        Config.ballSpeedX = json.GetFloat("ball_speed_x", 180.0f);
        Config.ballSpeedY = json.GetFloat("ball_speed_y", -200.0f);
        Config.ballMaxSpeed = json.GetFloat("ball_max_speed", 400.0f);
        Config.paddleWidth = json.GetFloat("paddle_width", 120.0f);
        Config.paddleHeight = json.GetFloat("paddle_height", 15.0f);
        Config.paddleSpeed = json.GetFloat("paddle_speed", 400.0f);
        Config.initialLives = json.GetInt("initial_lives", 3);
        Config.scorePerBrick = json.GetInt("score_per_brick", 10);
        Config.powerUpDropRate = json.GetFloat("powerup_drop_rate", 0.3f);
        Config.totalLevels = json.GetInt("total_levels", 5);

        std::cout << "配置加载成功" << std::endl;
        return true;
    }

    static void SaveDefaultConfig(const std::string& path) {
        std::ofstream file(path);
        file << "{\n";
        file << "    \"ball_radius\": 8,\n";
        file << "    \"ball_speed_x\": 180,\n";
        file << "    \"ball_speed_y\": -200,\n";
        file << "    \"ball_max_speed\": 400,\n";
        file << "    \"paddle_width\": 120,\n";
        file << "    \"paddle_height\": 15,\n";
        file << "    \"paddle_speed\": 400,\n";
        file << "    \"initial_lives\": 3,\n";
        file << "    \"score_per_brick\": 10,\n";
        file << "    \"powerup_drop_rate\": 0.3,\n";
        file << "    \"total_levels\": 5\n";
        file << "}\n";
    }
};

// ======================== 存档系统 ========================
class SaveSystem {
public:
    static bool SaveGame(const SaveData& data, const std::string& path = "saves/save.json") {
        try {
            _mkdir("saves");

            std::ofstream file(path);
            if (!file.is_open()) return false;

            file << "{\n";
            file << "    \"version\": " << data.version << ",\n";
            file << "    \"current_level\": " << data.currentLevel << ",\n";
            file << "    \"score\": " << data.score << ",\n";
            file << "    \"lives\": " << data.lives << ",\n";
            file << "    \"slow_effect_remaining\": " << data.slowEffectRemaining << ",\n";
            file << "    \"paddle_effect_remaining\": " << data.paddleEffectRemaining << ",\n";
            file << "    \"save_time\": " << time(nullptr) << "\n";
            file << "}\n";

            std::cout << "游戏已保存: Level " << data.currentLevel
                << ", Score " << data.score << std::endl;
            return true;
        }
        catch (...) {
            return false;
        }
    }

    static bool LoadGame(SaveData& data, const std::string& path = "saves/save.json") {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::stringstream buffer;
        buffer << file.rdbuf();

        SimpleJSON json;
        if (!json.Parse(buffer.str())) return false;

        data.version = json.GetInt("version", 1);
        if (data.version == 1) {
            data.currentLevel = json.GetInt("current_level", 1);
            data.score = json.GetInt("score", 0);
            data.lives = json.GetInt("lives", 3);
            data.slowEffectRemaining = json.GetFloat("slow_effect_remaining", 0.0f);
            data.paddleEffectRemaining = json.GetFloat("paddle_effect_remaining", 0.0f);
            std::cout << "存档加载成功: Level " << data.currentLevel << std::endl;
            return true;
        }
        return false;
    }

    static bool SaveExists(const std::string& path = "saves/save.json") {
        std::ifstream file(path);
        return file.good();
    }

    static void DeleteSave(const std::string& path = "saves/save.json") {
        std::remove(path.c_str());
    }
};

// ======================== 关卡加载器 ========================
class LevelLoader {
public:
    static SimpleJSON LoadLevelConfig(int level, const std::string& levelsDir = "levels/") {
        std::string filename = levelsDir + "level" + std::to_string(level) + ".json";
        SimpleJSON emptyConfig;

        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cout << "关卡文件不存在: " << filename << "，使用默认布局" << std::endl;
            return GetDefaultLevel(level);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        SimpleJSON config;
        if (!config.Parse(buffer.str())) {
            std::cout << "JSON解析失败，使用默认布局" << std::endl;
            return GetDefaultLevel(level);
        }

        std::cout << "关卡 " << level << " 加载成功" << std::endl;
        return config;
    }

    static SimpleJSON GetDefaultLevel(int level) {
        SimpleJSON config;
        // 简单返回一个空配置，实际使用时通过ParseObject填充
        return config;
    }

    static std::vector<Brick> CreateBricksFromLevel(int level) {
        std::vector<Brick> bricks;

        // 根据关卡难度生成砖块
        int rows = 5 + (level - 1) / 2;
        int cols = 12;
        float brickWidth = 60;
        float brickHeight = 20;
        float padding = 5;

        float totalWidth = cols * (brickWidth + padding);
        float startX = (SCREEN_WIDTH - totalWidth) / 2;
        float startY = 80;

        for (int row = 0; row < rows; row++) {
            Color color;
            int hp;
            int scoreValue;

            // 根据关卡和行数决定砖块强度
            if (level == 1) {
                // 第一关：简单
                Color colors[] = { RED, ORANGE, YELLOW, GREEN, SKYBLUE };
                color = colors[row % 5];
                hp = 1;
                scoreValue = 10;
            }
            else if (level == 2) {
                // 第二关：中等
                if (row < 2) { color = RED; hp = 1; scoreValue = 10; }
                else if (row < 4) { color = YELLOW; hp = 2; scoreValue = 20; }
                else { color = GREEN; hp = 3; scoreValue = 30; }
            }
            else if (level == 3) {
                // 第三关：困难
                if (row < 2) { color = ORANGE; hp = 2; scoreValue = 20; }
                else if (row < 4) { color = SKYBLUE; hp = 3; scoreValue = 30; }
                else { color = PURPLE; hp = 4; scoreValue = 40; }
            }
            else {
                // 第四、五关：专家
                if (row < 2) { color = SKYBLUE; hp = 3; scoreValue = 30; }
                else { color = PURPLE; hp = 4; scoreValue = 40; }
            }

            for (int col = 0; col < cols; col++) {
                // 创建有趣的图案
                if (level >= 4) {
                    // 专家关卡：中间有空隙
                    if (col > 2 && col < cols - 3 && (row % 2 == 0)) {
                        continue; // 创建空洞
                    }
                }

                float x = startX + col * (brickWidth + padding);
                float y = startY + row * (brickHeight + padding);
                bricks.push_back(Brick(x, y, brickWidth, brickHeight, color, hp, scoreValue));
            }
        }

        return bricks;
    }
};

// ======================== 游戏主类 ========================
class Game {
private:
    std::vector<Ball> balls;
    Paddle paddle;
    std::vector<Brick> bricks;
    std::vector<PowerUp> powerUps;
    ParticleSystem particles;

    GameState state;
    int lives;
    int score;
    int currentLevel;
    int totalLevels;
    bool gameRunning;
    float slowEffectRemaining;

    SaveData saveData;
    bool editingMode;

    // 性能优化：网格
    static const int GRID_COLS = 6;
    static const int GRID_ROWS = 8;
    std::vector<Brick*> grid[GRID_COLS][GRID_ROWS];
    float cellWidth, cellHeight;

public:
    Game() : state(GameState::MENU), lives(Config.initialLives), score(0),
        currentLevel(1), totalLevels(Config.totalLevels), gameRunning(true),
        slowEffectRemaining(0), editingMode(false) {

        srand((unsigned int)time(nullptr));
        cellWidth = SCREEN_WIDTH / (float)GRID_COLS;
        cellHeight = SCREEN_HEIGHT / (float)GRID_ROWS;

        // 创建必要目录
        _mkdir("levels");
        _mkdir("saves");

        // 加载配置
        if (!ConfigLoader::LoadConfig("config.json")) {
            ConfigLoader::SaveDefaultConfig("config.json");
        }

        // 生成示例关卡文件
        GenerateExampleLevels();
    }

    void GenerateExampleLevels() {
        // 生成关卡1配置文件
        std::ofstream level1("levels/level1.json");
        level1 << R"({
    "level": 1,
    "name": "入门关卡",
    "bricks": {
        "rows": 5,
        "cols": 12,
        "width": 60,
        "height": 20,
        "padding": 5,
        "layout": [
            [1,1,1,1,1,1,1,1,1,1,1,1],
            [1,1,1,1,1,1,1,1,1,1,1,1],
            [1,1,1,1,1,1,1,1,1,1,1,1],
            [1,1,1,1,1,1,1,1,1,1,1,1],
            [1,1,1,1,1,1,1,1,1,1,1,1]
        ]
    }
})";
        level1.close();

        // 关卡2
        std::ofstream level2("levels/level2.json");
        level2 << R"({
    "level": 2,
    "name": "进阶关卡",
    "bricks": {
        "rows": 6,
        "cols": 12,
        "width": 60,
        "height": 20,
        "padding": 5,
        "layout": [
            [1,1,1,1,1,1,1,1,1,1,1,1],
            [1,1,1,1,1,1,1,1,1,1,1,1],
            [2,2,2,2,2,2,2,2,2,2,2,2],
            [2,2,2,2,2,2,2,2,2,2,2,2],
            [3,3,3,3,3,3,3,3,3,3,3,3],
            [3,3,3,3,3,3,3,3,3,3,3,3]
        ]
    }
})";
        level2.close();

        // 关卡3
        std::ofstream level3("levels/level3.json");
        level3 << R"({
    "level": 3,
    "name": "挑战关卡",
    "bricks": {
        "rows": 7,
        "cols": 12,
        "width": 60,
        "height": 20,
        "padding": 5,
        "layout": [
            [1,1,1,0,0,0,0,0,0,1,1,1],
            [1,1,1,0,0,0,0,0,0,1,1,1],
            [2,2,2,2,0,0,0,0,2,2,2,2],
            [2,2,2,2,0,0,0,0,2,2,2,2],
            [3,3,3,3,3,3,3,3,3,3,3,3],
            [3,3,3,3,3,3,3,3,3,3,3,3],
            [4,4,4,4,4,4,4,4,4,4,4,4]
        ]
    }
})";
        level3.close();

        std::cout << "示例关卡文件已生成" << std::endl;
    }

    void InitGame() {
        balls.clear();
        balls.push_back(Ball());
        paddle.Reset();
        LoadLevel(currentLevel);
        powerUps.clear();
        particles.Clear();

        score = 0;
        lives = Config.initialLives;
        slowEffectRemaining = 0;
        state = GameState::PLAYING;
    }

    void LoadLevel(int level) {
        bricks.clear();

        // 使用关卡加载器创建砖块
        bricks = LevelLoader::CreateBricksFromLevel(level);

        std::cout << "加载关卡 " << level << "，共 " << bricks.size() << " 个砖块" << std::endl;

        if (!balls.empty()) {
            balls[0].Reset();
        }
        paddle.Reset();
    }

    void SaveProgress() {
        saveData.currentLevel = currentLevel;
        saveData.score = score;
        saveData.lives = lives;
        saveData.slowEffectRemaining = slowEffectRemaining;
        saveData.paddleEffectRemaining = paddle.effectRemainingTime;
        SaveSystem::SaveGame(saveData);
    }

    bool LoadProgress() {
        if (!SaveSystem::SaveExists()) return false;

        if (SaveSystem::LoadGame(saveData)) {
            currentLevel = saveData.currentLevel;
            score = saveData.score;
            lives = saveData.lives;
            slowEffectRemaining = saveData.slowEffectRemaining;

            if (saveData.paddleEffectRemaining > 0) {
                paddle.Extend(Config.paddleExtendWidth, saveData.paddleEffectRemaining);
            }

            LoadLevel(currentLevel);

            if (slowEffectRemaining > 0 && !balls.empty()) {
                balls[0].ApplySlow(Config.slowBallFactor);
            }

            return true;
        }
        return false;
    }

    void Update(float dt) {
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
        case GameState::EDIT_MODE:
            UpdateEditMode(dt);
            break;
        }
    }

    void UpdateMenu() {
        if (IsKeyPressed(KEY_ENTER)) {
            if (SaveSystem::SaveExists()) {
                if (LoadProgress()) {
                    state = GameState::PLAYING;
                }
                else {
                    currentLevel = 1;
                    InitGame();
                }
            }
            else {
                currentLevel = 1;
                InitGame();
            }
        }
        if (IsKeyPressed(KEY_N)) {
            currentLevel = 1;
            InitGame();
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            gameRunning = false;
        }
    }

    void UpdatePlaying(float dt) {
        // 编辑模式切换
        if (IsKeyPressed(KEY_E)) {
            editingMode = !editingMode;
            state = editingMode ? GameState::EDIT_MODE : GameState::PLAYING;
            return;
        }

        // 更新减速效果
        if (slowEffectRemaining > 0) {
            slowEffectRemaining -= dt;
            if (slowEffectRemaining <= 0) {
                for (auto& ball : balls) {
                    ball.speed.x /= Config.slowBallFactor;
                    ball.speed.y /= Config.slowBallFactor;
                }
            }
        }

        paddle.Update(dt);

        for (auto& ball : balls) {
            ball.Update(dt);
            if (ball.active && (rand() % 10 < 3)) {
                particles.SpawnTrail(ball.position, WHITE);
            }
        }

        for (auto& powerUp : powerUps) {
            powerUp.Update(dt);
        }

        particles.Update(dt);
        CheckCollisions();
        CheckBallsOut();
        CheckVictory();

        // 清理失效道具
        powerUps.erase(
            std::remove_if(powerUps.begin(), powerUps.end(),
                [](const PowerUp& p) { return !p.active || p.position.y > SCREEN_HEIGHT + 50; }),
            powerUps.end()
        );

        // 清理失效球
        balls.erase(
            std::remove_if(balls.begin(), balls.end(),
                [](const Ball& b) { return !b.active; }),
            balls.end()
        );

        // 处理球丢失
        if (balls.empty()) {
            lives--;
            if (lives <= 0) {
                state = GameState::GAMEOVER;
                SaveSystem::DeleteSave();
            }
            else {
                balls.push_back(Ball());
                paddle.Reset();
                SaveProgress();
            }
        }

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
            currentLevel = 1;
            InitGame();
        }
    }

    void UpdateEditMode(float dt) {
        // 编辑模式下正常游戏
        UpdatePlaying(dt);

        // 编辑模式控制
        Vector2 mouse = GetMousePosition();
        float brickWidth = 60;
        float brickHeight = 20;
        float padding = 5;
        float startX = (SCREEN_WIDTH - 12 * (brickWidth + padding)) / 2;
        float startY = 80;

        int gridCol = (int)((mouse.x - startX) / (brickWidth + padding));
        int gridRow = (int)((mouse.y - startY) / (brickHeight + padding));

        // 左键添加砖块
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (gridRow >= 0 && gridRow < 8 && gridCol >= 0 && gridCol < 12) {
                bricks.push_back(Brick(
                    startX + gridCol * (brickWidth + padding),
                    startY + gridRow * (brickHeight + padding),
                    brickWidth, brickHeight,
                    RED, 1, 10
                ));
            }
        }

        // 右键删除砖块
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            for (auto it = bricks.begin(); it != bricks.end(); ++it) {
                if (it->active && CheckCollisionPointRec(mouse, it->rect)) {
                    bricks.erase(it);
                    break;
                }
            }
        }

        // 保存自定义关卡
        if (IsKeyPressed(KEY_S)) {
            SaveCustomLevel();
        }
    }

    void SaveCustomLevel() {
        std::ofstream file("levels/custom.json");
        if (!file.is_open()) {
            std::cout << "无法保存自定义关卡" << std::endl;
            return;
        }

        file << "{\n";
        file << "    \"level\": 0,\n";
        file << "    \"name\": \"Custom Level\",\n";
        file << "    \"bricks\": {\n";
        file << "        \"rows\": 8,\n";
        file << "        \"cols\": 12,\n";
        file << "        \"width\": 60,\n";
        file << "        \"height\": 20,\n";
        file << "        \"padding\": 5,\n";
        file << "        \"layout\": [\n";

        // 计算布局
        float brickWidth = 60;
        float brickHeight = 20;
        float padding = 5;
        float startX = (SCREEN_WIDTH - 12 * (brickWidth + padding)) / 2;
        float startY = 80;

        for (int row = 0; row < 8; row++) {
            file << "            [";
            for (int col = 0; col < 12; col++) {
                bool hasBrick = false;
                for (const auto& brick : bricks) {
                    if (brick.active &&
                        fabs(brick.rect.x - (startX + col * (brickWidth + padding))) < 1 &&
                        fabs(brick.rect.y - (startY + row * (brickHeight + padding))) < 1) {
                        hasBrick = true;
                        break;
                    }
                }
                file << (hasBrick ? "1" : "0");
                if (col < 11) file << ",";
            }
            file << "]";
            if (row < 7) file << ",";
            file << "\n";
        }

        file << "        ]\n";
        file << "    }\n";
        file << "}\n";

        std::cout << "自定义关卡已保存到 levels/custom.json" << std::endl;
    }

    void CheckCollisions() {
        UpdateSpatialGrid();

        // 球与挡板碰撞
        for (auto& ball : balls) {
            if (CheckCollisionCircleRec(ball.position, ball.radius, paddle.GetRect())) {
                ball.speed.y = -fabs(ball.speed.y);
                float offset = ball.position.x - (paddle.rect.x + paddle.rect.width / 2);
                ball.speed.x += offset * 5.0f;

                if (ball.speed.x > Config.ballMaxSpeed) ball.speed.x = Config.ballMaxSpeed;
                if (ball.speed.x < -Config.ballMaxSpeed) ball.speed.x = -Config.ballMaxSpeed;
                if (ball.speed.y > Config.ballMaxSpeed) ball.speed.y = Config.ballMaxSpeed;

                particles.SpawnExplosion(ball.position, YELLOW);
            }
        }

        // 球与砖块碰撞
        for (auto& ball : balls) {
            for (auto& brick : bricks) {
                if (brick.active && CheckCollisionCircleRec(ball.position, ball.radius, brick.rect)) {
                    bool destroyed = brick.Hit();

                    if (destroyed) {
                        score += brick.scoreValue;
                        particles.SpawnExplosion(
                            { brick.rect.x + brick.rect.width / 2,
                             brick.rect.y + brick.rect.height / 2 },
                            brick.color
                        );

                        // 随机生成道具
                        if ((rand() % 100) / 100.0f < Config.powerUpDropRate) {
                            PowerUpType type = static_cast<PowerUpType>(rand() % 4);
                            powerUps.push_back(PowerUp(
                                brick.rect.x + brick.rect.width / 2,
                                brick.rect.y + brick.rect.height / 2,
                                type
                            ));
                        }
                    }

                    // 球反弹
                    float overlapLeft = (ball.position.x + ball.radius) - brick.rect.x;
                    float overlapRight = (brick.rect.x + brick.rect.width) - (ball.position.x - ball.radius);
                    float overlapTop = (ball.position.y + ball.radius) - brick.rect.y;
                    float overlapBottom = (brick.rect.y + brick.rect.height) - (ball.position.y - ball.radius);

                    float minOverlap = std::min({ overlapLeft, overlapRight, overlapTop, overlapBottom });
                    if (minOverlap == overlapLeft || minOverlap == overlapRight) {
                        ball.speed.x = -ball.speed.x;
                    }
                    else {
                        ball.speed.y = -ball.speed.y;
                    }

                    goto next_ball;
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
        for (int i = 0; i < GRID_COLS; i++) {
            for (int j = 0; j < GRID_ROWS; j++) {
                grid[i][j].clear();
            }
        }

        for (auto& brick : bricks) {
            if (brick.active) {
                int gridX = (int)(brick.rect.x / cellWidth);
                int gridY = (int)(brick.rect.y / cellHeight);
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
            if (currentLevel < totalLevels) {
                currentLevel++;
                LoadLevel(currentLevel);
                SaveProgress();
                std::cout << "通关！进入第 " << currentLevel << " 关" << std::endl;
            }
            else {
                state = GameState::VICTORY;
                SaveSystem::DeleteSave();
            }
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
        case GameState::EDIT_MODE:
            DrawEditMode();
            break;
        }

        EndDrawing();
    }

    void DrawGame() {
        for (const auto& brick : bricks) brick.Draw();
        paddle.Draw();
        for (const auto& ball : balls) ball.Draw();
        for (const auto& powerUp : powerUps) powerUp.Draw();
        particles.Draw();
        DrawUI();

        if (state == GameState::PAUSED) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.7f));
            DrawText("PAUSED", SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2, 40, WHITE);
            DrawText("Press P to resume", SCREEN_WIDTH / 2 - 110, SCREEN_HEIGHT / 2 + 50, 20, WHITE);
        }
    }

    void DrawUI() {
        DrawText(TextFormat("Score: %d", score), 10, 10, 25, WHITE);
        DrawText(TextFormat("Level: %d", currentLevel), 10, 40, 20, YELLOW);

        for (int i = 0; i < lives; i++) {
            DrawCircle(30 + i * 35, 70, 12, RED);
            DrawText("❤", 27 + i * 35, 63, 15, WHITE);
        }

        DrawText(TextFormat("FPS: %d", GetFPS()), SCREEN_WIDTH - 80, 10, 15, GREEN);

        int remaining = 0;
        for (const auto& brick : bricks) if (brick.active) remaining++;
        DrawText(TextFormat("Bricks: %d", remaining), SCREEN_WIDTH - 90, 30, 12, ORANGE);

        if (slowEffectRemaining > 0) {
            DrawText(TextFormat("SLOW: %.1f", slowEffectRemaining),
                SCREEN_WIDTH / 2 - 50, 10, 15, PURPLE);
        }
        if (paddle.effectRemainingTime > 0) {
            DrawText(TextFormat("EXTEND: %.1f", paddle.effectRemainingTime),
                SCREEN_WIDTH / 2 - 60, 30, 15, BLUE);
        }

        DrawText("E: Edit Mode", SCREEN_WIDTH - 120, SCREEN_HEIGHT - 25, 12, GRAY);
    }

    void DrawMenu() {
        const char* title = "BREAKOUT";
        const char* subtitle = "Data Persistent & Level Editor";
        const char* continueGame = "Press ENTER to Continue / Load Save";
        const char* newGame = "Press N for New Game";
        const char* quit = "Press ESC to Quit";

        int titleWidth = MeasureText(title, 60);
        int subtitleWidth = MeasureText(subtitle, 20);

        float time = GetTime();
        Color titleColor = { 255, (unsigned char)(128 + sin(time) * 127), 0, 255 };

        DrawText(title, SCREEN_WIDTH / 2 - titleWidth / 2, SCREEN_HEIGHT / 3 - 30, 60, titleColor);
        DrawText(subtitle, SCREEN_WIDTH / 2 - subtitleWidth / 2, SCREEN_HEIGHT / 3 + 30, 20, GRAY);
        DrawText(continueGame, SCREEN_WIDTH / 2 - MeasureText(continueGame, 25) / 2, SCREEN_HEIGHT / 2, 25, YELLOW);
        DrawText(newGame, SCREEN_WIDTH / 2 - MeasureText(newGame, 20) / 2, SCREEN_HEIGHT / 2 + 50, 20, WHITE);
        DrawText(quit, SCREEN_WIDTH / 2 - MeasureText(quit, 20) / 2, SCREEN_HEIGHT / 2 + 90, 20, GRAY);

        if (SaveSystem::SaveExists()) {
            DrawText("★ Save file detected! ★", SCREEN_WIDTH / 2 - 130, SCREEN_HEIGHT / 2 - 40, 20, GREEN);
        }

        DrawText("CONTROLS:", 50, SCREEN_HEIGHT - 120, 20, WHITE);
        DrawText("LEFT/RIGHT - Move paddle", 50, SCREEN_HEIGHT - 95, 15, GRAY);
        DrawText("P - Pause", 50, SCREEN_HEIGHT - 75, 15, GRAY);
        DrawText("E - Edit Mode (in game)", 50, SCREEN_HEIGHT - 55, 15, GRAY);
        DrawText("ESC - Quit", 50, SCREEN_HEIGHT - 35, 15, GRAY);

        DrawText("FEATURES:", SCREEN_WIDTH - 180, SCREEN_HEIGHT - 120, 20, WHITE);
        DrawText("JSON Config Drive", SCREEN_WIDTH - 180, SCREEN_HEIGHT - 95, 12, GREEN);
        DrawText("Save/Load System", SCREEN_WIDTH - 180, SCREEN_HEIGHT - 80, 12, GREEN);
        DrawText("Multi-Level (5)", SCREEN_WIDTH - 180, SCREEN_HEIGHT - 65, 12, GREEN);
        DrawText("Edit Mode", SCREEN_WIDTH - 180, SCREEN_HEIGHT - 50, 12, GREEN);
        DrawText("Power-Ups", SCREEN_WIDTH - 180, SCREEN_HEIGHT - 35, 12, GREEN);
    }

    void DrawGameOver() {
        DrawGame();
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));

        DrawText("GAME OVER", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 3, 55, RED);
        DrawText("Press ENTER to Restart", SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2, 25, WHITE);
        DrawText(TextFormat("Final Score: %d", score),
            SCREEN_WIDTH / 2 - MeasureText(TextFormat("Final Score: %d", score), 30) / 2,
            SCREEN_HEIGHT / 2 + 50, 30, YELLOW);
    }

    void DrawVictory() {
        DrawGame();
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));

        float time = GetTime();
        Color victoryColor = { 0, (unsigned char)(128 + sin(time) * 127), 0, 255 };

        DrawText("VICTORY!", SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 3, 65, victoryColor);
        DrawText("You completed all levels!", SCREEN_WIDTH / 2 - 140, SCREEN_HEIGHT / 2, 25, WHITE);
        DrawText("Press ENTER to Play Again", SCREEN_WIDTH / 2 - 130, SCREEN_HEIGHT / 2 + 50, 20, YELLOW);
        DrawText(TextFormat("Final Score: %d", score),
            SCREEN_WIDTH / 2 - MeasureText(TextFormat("Final Score: %d", score), 30) / 2,
            SCREEN_HEIGHT / 2 + 100, 30, YELLOW);
    }

    void DrawEditMode() {
        DrawGame();

        // 绘制编辑模式UI
        DrawRectangle(0, 0, SCREEN_WIDTH, 40, ColorAlpha(BLACK, 0.8f));
        DrawText("EDIT MODE", 10, 10, 20, RED);
        DrawText("Left Click: Add Brick | Right Click: Remove Brick | S: Save Custom Level | E: Exit Edit Mode",
            SCREEN_WIDTH / 2 - 350, 10, 20, YELLOW);

        // 绘制网格
        float brickWidth = 60;
        float brickHeight = 20;
        float padding = 5;
        float startX = (SCREEN_WIDTH - 12 * (brickWidth + padding)) / 2;
        float startY = 80;

        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 12; col++) {
                float x = startX + col * (brickWidth + padding);
                float y = startY + row * (brickHeight + padding);
                DrawRectangleLines(x, y, brickWidth, brickHeight, DARKGRAY);
            }
        }

        Vector2 mouse = GetMousePosition();
        DrawCircleV(mouse, 5, ColorAlpha(RED, 0.5f));
    }

    bool IsRunning() const { return gameRunning; }
};

// ======================== 主函数 ========================
int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Breakout - Data Persistent & Level Editor");
    SetTargetFPS(TARGET_FPS);
    InitAudioDevice();

    Game game;

    while (!WindowShouldClose() && game.IsRunning()) {
        float dt = GetFrameTime();
        if (dt > 0.033f) dt = 0.033f;

        game.Update(dt);
        game.Draw();
    }

    CloseAudioDevice();
    CloseWindow();

    return 0;
}
