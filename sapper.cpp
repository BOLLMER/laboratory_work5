#include <SFML/Graphics.hpp>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
#include <memory>
#include <cmath>
#include <cstdio>
// Вперёд объявляем Game, чтобы состояния могли на него ссылаться
class Game;

// CONTENT LAYER (Mine / Number / Empty)
// Здесь мы делаем "что лежит внутри клетки".

class ICellContent {
public:
    virtual ~ICellContent() = default;
    virtual bool isMine() const = 0;
    virtual int  number() const = 0;   // Mine:-1, otherwise:0..8
    virtual bool isEmpty() const = 0;
};

class MineContent final : public ICellContent {
public:
    bool isMine() const override { return true; }
    int  number() const override { return -1; }
    bool isEmpty() const override { return false; }
};

class NumberContent final : public ICellContent {
    int n_;
public:
    explicit NumberContent(int n) : n_(n) {}
    bool isMine() const override { return false; }
    int  number() const override { return n_; }
    bool isEmpty() const override { return n_ == 0; }
};

class EmptyContent final : public ICellContent {
public:
    bool isMine() const override { return false; }
    int  number() const override { return 0; }
    bool isEmpty() const override { return true; }
};

// Factory Method (фабричный метод) для создания контента. Это часть фабричного подхода: Game просит "создай мину/число",а не пишет "new MineContent()" напрямую.
struct CellContentFactory {
    static std::unique_ptr<ICellContent> makeMine()   { return std::make_unique<MineContent>(); }
    static std::unique_ptr<ICellContent> makeEmpty()  { return std::make_unique<EmptyContent>(); }
    static std::unique_ptr<ICellContent> makeNumber(int n) {
        if (n <= 0) return makeEmpty();               // 0 -> EmptyContent
        return std::make_unique<NumberContent>(n);
    }
};

// STATE LAYER (Closed / Opened / Flagged) — PATTERN: STATE

class ICellState {
public:
    virtual ~ICellState() = default;

    // PATTERN State:
    // Одинаковый интерфейс, но разные реализации поведения.
    virtual void onLeftClick(Game& game, int x, int y) = 0;
    virtual void onRightClick(Game& game, int x, int y) = 0;

    // Для рендера и логики:
    virtual bool isOpen() const = 0;
    virtual bool isFlagged() const = 0;
};

struct Cell {
    std::unique_ptr<ICellContent> content;//(мина/число/пусто)
    std::unique_ptr<ICellState>   state;//(закрыто/открыто/флаг)
};

// ABSTRACT FACTORY — PATTERN: Abstract Factory

class ICellFactory {
public:
    virtual ~ICellFactory() = default;

    // Создать "стартовую" клетку (пустую и закрытую)
    virtual Cell makeInitialCell() const = 0;

    // Создать контент
    virtual std::unique_ptr<ICellContent> makeMineContent() const = 0;
    virtual std::unique_ptr<ICellContent> makeNumberContent(int n) const = 0;
};

class DefaultCellFactory final : public ICellFactory {
public:
    Cell makeInitialCell() const override; // определим ниже (нужны фабрики состояний)

    std::unique_ptr<ICellContent> makeMineContent() const override {
        return CellContentFactory::makeMine();
    }
    std::unique_ptr<ICellContent> makeNumberContent(int n) const override {
        return CellContentFactory::makeNumber(n);
    }
};

// STRATEGY — PATTERN: Strategy (генерация поля)

class IBoardGenerator {
public:
    virtual ~IBoardGenerator() = default;
    virtual void generate(Game& game, int safeX, int safeY) = 0;
};

// GAME LOGIC (почти без SFML)

class Game {
public:
    int W, H, MINES;

    bool gameOver   = false;
    bool win        = false;
    bool firstClick = true;

    // небольшая анимация взрыва
    bool  explosion = false;
    float explosionTimer = 0.0f;

    // таймер с первого клика
    bool  timerRunning = false;
    float timeElapsed = 0.0f;

    std::vector<std::vector<Cell>> field;

    // Dependency Injection: внедряем фабрики/стратегии извне
    std::unique_ptr<ICellFactory>    cellFactory;     // Abstract Factory
    std::unique_ptr<IBoardGenerator> boardGenerator;  // Strategy

    Game(int w, int h, int mines,
         std::unique_ptr<ICellFactory> cf,
         std::unique_ptr<IBoardGenerator> bg)
        : W(w), H(h), MINES(mines), cellFactory(std::move(cf)), boardGenerator(std::move(bg))
    {
        resetField();
    }

    // Фабрики состояний (упрощают переключение)
    static std::unique_ptr<ICellState> makeClosedState();
    static std::unique_ptr<ICellState> makeOpenedState();
    static std::unique_ptr<ICellState> makeFlaggedState();

    void resetField() {
        // Создаём поле, используя Abstract Factory
        field.clear();
        field.resize(H);
        for (int y = 0; y < H; ++y) {
            field[y].reserve(W);
            for (int x = 0; x < W; ++x) {
                field[y].push_back(cellFactory->makeInitialCell());
            }
        }

        // Сбрасываем флаги игры
        gameOver = false;
        win = false;
        firstClick = true;

        explosion = false;
        explosionTimer = 0.0f;

        timerRunning = false;
        timeElapsed = 0.0f;
    }

    void update(float dt) {
        // Логика "анимации" взрыва (не SFML-рендер, а просто таймер состояния)
        if (explosion) {
            explosionTimer -= dt;
            if (explosionTimer < 0) explosion = false;
        }
        // Таймер игры
        if (timerRunning && !gameOver && !win) timeElapsed += dt;
    }

    void startTimerIfNeeded() {
        if (!timerRunning) { timerRunning = true; timeElapsed = 0.0f; }
    }

    void stopTimer() { timerRunning = false; }

    // Ввод делегируется состоянию (State pattern)
    void leftClickCell(int x, int y)  { field[y][x].state->onLeftClick(*this, x, y); }
    void rightClickCell(int x, int y) { field[y][x].state->onRightClick(*this, x, y); }

    // Подсчёт мин вокруг (используется генератором)
    int countMinesAround(int x, int y) const {
        int cnt = 0;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (!(dx == 0 && dy == 0)) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < W && ny >= 0 && ny < H)
                        if (field[ny][nx].content && field[ny][nx].content->isMine())
                            cnt++;
                }
        return cnt;
    }

    void revealFromState(int x, int y) {// открыть клетку (из State)
        if (gameOver || win) return;

        Cell &c = field[y][x];

        // нельзя открыть открытую/флажок
        if (c.state->isOpen() || c.state->isFlagged()) return;

        // 1-й клик: генерация поля (Strategy)
        if (firstClick) {
            boardGenerator->generate(*this, x, y); // Strategy usage
            firstClick = false;
            startTimerIfNeeded();
        }

        // Открываем клетку (State switching)
        c.state = makeOpenedState();

        // Если мина — проигрыш
        if (c.content->isMine()) {
            triggerExplosion();
            return;
        }

        // Если пусто — flood fill (раскрытие области)
        if (c.content->isEmpty()) {
            floodFill(x, y);
        }

        checkWin();
    }

    void toggleFlagFromState(int x, int y) {// поставить/снять флаг(из State)
        if (gameOver || win) return;

        Cell &c = field[y][x];

        // на открытой клетке флаг не ставим
        if (c.state->isOpen()) return;

        // State switching: Closed <-> Flagged
        if (c.state->isFlagged()) c.state = makeClosedState();
        else c.state = makeFlaggedState();

        checkWin();
    }

    void floodFill(int x, int y) {
        // Рекурсивно открываем соседей у нулевых клеток
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (!(dx == 0 && dy == 0)) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < W && ny >= 0 && ny < H) {
                        Cell &c = field[ny][nx];

                        // не открываем мины, флаги и уже открытое
                        if (!c.state->isOpen() && !c.state->isFlagged() && !c.content->isMine()) {
                            c.state = makeOpenedState();
                            if (c.content->isEmpty()) floodFill(nx, ny);
                        }
                    }
                }
    }

    void triggerExplosion() {
        // логика проигрыша
        explosion = true;
        explosionTimer = 0.2f;

        // раскрываем всё поле
        for (int yy = 0; yy < H; yy++)
            for (int xx = 0; xx < W; xx++)
                field[yy][xx].state = makeOpenedState();

        gameOver = true;
        stopTimer();
    }

    int flagsCount() const {
        int f = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (field[y][x].state->isFlagged())
                    f++;
        return f;
    }

    void checkWinOpen() {
        // Победа по открытым клеткам
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (!field[y][x].content->isMine() && !field[y][x].state->isOpen())
                    return;
        win = true;
        stopTimer();
    }

    void checkWinFlags() {
        // Победа по флагам: ровно MINES флагов и все на минах
        int flagged = 0;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                if (field[y][x].state->isFlagged()) {
                    flagged++;
                    if (!field[y][x].content->isMine()) return; // ошибка
                }
            }
        }
        if (flagged == MINES) {
            win = true;
            stopTimer();
        }
    }

    void checkWin() {
        checkWinOpen();
        if (!win) checkWinFlags();
    }
};

// Board generator implementation (Strategy concrete)

class DefaultBoardGenerator final : public IBoardGenerator {
public:
    void generate(Game& game, int safeX, int safeY) override {
        // очистить контент
        for (int y = 0; y < game.H; y++)
            for (int x = 0; x < game.W; x++)
                game.field[y][x].content = game.cellFactory->makeNumberContent(0);

        // поставить мины (не в safe зоне)
        int placed = 0;
        std::srand((unsigned)std::time(nullptr));

        while (placed < game.MINES) {
            int x = std::rand() % game.W;
            int y = std::rand() % game.H;

            if (game.field[y][x].content->isMine()) continue;
            if (std::abs(x - safeX) <= 1 && std::abs(y - safeY) <= 1) continue;

            game.field[y][x].content = game.cellFactory->makeMineContent();
            placed++;
        }

        // рассчитать числа
        for (int y = 0; y < game.H; y++) {
            for (int x = 0; x < game.W; x++) {
                if (game.field[y][x].content->isMine()) continue;
                int around = game.countMinesAround(x, y);
                game.field[y][x].content = game.cellFactory->makeNumberContent(around);
            }
        }
    }
};

// State implementations (State pattern concrete states)

class ClosedState final : public ICellState {
public:
    void onLeftClick(Game& game, int x, int y) override {
        // State pattern: закрытая клетка реагирует на ЛКМ как "открыть"
        game.revealFromState(x, y);
    }
    void onRightClick(Game& game, int x, int y) override {
        // State pattern: закрытая клетка реагирует на ПКМ как "флаг"
        game.toggleFlagFromState(x, y);
    }
    bool isOpen() const override { return false; }
    bool isFlagged() const override { return false; }
};

class OpenedState final : public ICellState {
public:
    void onLeftClick(Game&, int, int) override {}
    void onRightClick(Game&, int, int) override {}
    bool isOpen() const override { return true; }
    bool isFlagged() const override { return false; }
};

class FlaggedState final : public ICellState {
public:
    void onLeftClick(Game&, int, int) override {}
    void onRightClick(Game& game, int x, int y) override {
        // State pattern: ПКМ на флаге -> снять флаг -> перейти в ClosedState
        game.field[y][x].state = Game::makeClosedState();
    }
    bool isOpen() const override { return false; }
    bool isFlagged() const override { return true; }
};

// Фабрики состояний (облегчают переключение состояния)
std::unique_ptr<ICellState> Game::makeClosedState()  { return std::make_unique<ClosedState>(); }
std::unique_ptr<ICellState> Game::makeOpenedState()  { return std::make_unique<OpenedState>(); }
std::unique_ptr<ICellState> Game::makeFlaggedState() { return std::make_unique<FlaggedState>(); }

// Abstract Factory: начальная клетка (пустая и закрытая)
Cell DefaultCellFactory::makeInitialCell() const {
    Cell c;
    c.content = CellContentFactory::makeEmpty();
    c.state   = Game::makeClosedState();
    return c;
}

// THEME (не паттерн строго, но вынесение параметров дизайна)

class ITheme {
public:
    virtual ~ITheme() = default;

    virtual sf::Color bgColor() const = 0;

    virtual sf::Color cellClosedColor() const = 0;
    virtual sf::Color cellFlagColor() const = 0;
    virtual sf::Color cellOpenedColor() const = 0;

    virtual sf::Color mineColor() const = 0;
    virtual sf::Color mineFlashColor() const = 0;

    virtual sf::Color statusColor() const = 0;
    virtual sf::Color numberColor(int n) const = 0;
    virtual sf::Color flagTextColor() const = 0;

    virtual const std::string& flagGlyph() const = 0;

    virtual int hudTitleSize() const = 0;
    virtual int hudSmallSize() const = 0;
    virtual int cellNumberSize() const = 0;
    virtual int cellFlagSize() const = 0;
};

class DefaultTheme final : public ITheme {
    std::string flag_ = "F";
public:
    sf::Color bgColor() const override { return sf::Color::White; }

    sf::Color cellClosedColor() const override { return sf::Color(100, 100, 100); }
    sf::Color cellFlagColor() const override { return sf::Color(255, 255, 0); }
    sf::Color cellOpenedColor() const override { return sf::Color(220, 220, 220); }

    sf::Color mineColor() const override { return sf::Color::Red; }
    sf::Color mineFlashColor() const override { return sf::Color::Yellow; }

    sf::Color statusColor() const override { return sf::Color::Red; }
    sf::Color numberColor(int) const override { return sf::Color::Blue; }
    sf::Color flagTextColor() const override { return sf::Color::Red; }

    const std::string& flagGlyph() const override { return flag_; }

    int hudTitleSize() const override { return 40; }
    int hudSmallSize() const override { return 22; }
    int cellNumberSize() const override { return 24; }
    int cellFlagSize() const override { return 24; }
};

// Factory (простая фабрика) для темы
struct ThemeFactory {
    static std::unique_ptr<ITheme> makeDefault() {
        return std::make_unique<DefaultTheme>();
    }
};

// SFML helpers: format time


static std::string formatTime(float sec) {
    int total = (int)sec;
    int m = total / 60;
    int s = total % 60;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
    return std::string(buf);
}

// Layout (геометрия интерфейса)

struct Layout {
    int WINDOW_W = 800;
    int WINDOW_H = 800;
    int CELL     = 40;
    int OFFSET_Y = 110;

    int boardWidthPx  = 0;
    int boardHeightPx = 0;
    int XOFFSET       = 0;

    void recompute(const Game& game) {
        boardWidthPx  = game.W * CELL;
        boardHeightPx = game.H * CELL;
        XOFFSET = (WINDOW_W - boardWidthPx) / 2;
        if (XOFFSET < 0) XOFFSET = 0;
    }
};

// UI widgets (SFML objects)

struct UiWidgets {
    sf::RectangleShape restartBtn;
    sf::Text           restartText;

    sf::RectangleShape menuBtn;
    sf::Text           menuText;

    sf::Text status;
    sf::Text minesIndicator;
    sf::Text timerText;
};

// RENDERER (SFML) — здесь используется SFML draw()

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void render(sf::RenderWindow& window, const Game& game, const Layout& layout, UiWidgets& ui) = 0;
};

class SfmlRenderer final : public IRenderer {
    sf::Font& font;
    const ITheme& theme;
public:
    SfmlRenderer(sf::Font& f, const ITheme& t) : font(f), theme(t) {}

    void render(sf::RenderWindow& window, const Game& game, const Layout& layout, UiWidgets& ui) override {
        // SFML: обновляем строки интерфейса
        if (game.win) ui.status.setString("YOU WIN!");
        else if (game.gameOver) ui.status.setString("YOU LOSE!");
        else ui.status.setString("");

        int remaining = game.MINES - game.flagsCount();
        ui.minesIndicator.setString("Mines: " + std::to_string(remaining));
        ui.timerText.setString("Time: " + formatTime(game.timeElapsed));

        // SFML: очистка окна (заливка фоном)
        window.clear(theme.bgColor());

        // SFML: рисуем поле
        for (int y = 0; y < game.H; y++) {
            for (int x = 0; x < game.W; x++) {
                sf::RectangleShape r(sf::Vector2f((float)layout.CELL - 2, (float)layout.CELL - 2));
                r.setPosition((float)layout.XOFFSET + x * layout.CELL + 1,
                              (float)layout.OFFSET_Y + y * layout.CELL + 1);

                const Cell &c = game.field[y][x];

                if (c.state->isOpen()) {
                    // SFML: цвет клетки зависит от контента
                    r.setFillColor(c.content->isMine()
                        ? (game.explosion ? theme.mineFlashColor() : theme.mineColor())
                        : theme.cellOpenedColor());
                    window.draw(r);

                    // SFML: рисуем число
                    if (!c.content->isMine() && c.content->number() > 0) {
                        sf::Text t;
                        t.setFont(font);
                        t.setString(std::to_string(c.content->number()));
                        t.setCharacterSize(theme.cellNumberSize());
                        t.setFillColor(theme.numberColor(c.content->number()));
                        t.setPosition((float)layout.XOFFSET + x * layout.CELL + 10,
                                      (float)layout.OFFSET_Y + y * layout.CELL + 5);
                        window.draw(t);
                    }
                } else {
                    // SFML: закрытая клетка, если флаг — другой цвет
                    r.setFillColor(c.state->isFlagged()
                        ? theme.cellFlagColor()
                        : theme.cellClosedColor());
                    window.draw(r);

                    // SFML: рисуем букву флага
                    if (c.state->isFlagged()) {
                        sf::Text f;
                        f.setFont(font);
                        f.setString(theme.flagGlyph());
                        f.setCharacterSize(theme.cellFlagSize());
                        f.setFillColor(theme.flagTextColor());
                        f.setPosition((float)layout.XOFFSET + x * layout.CELL + 10,
                                      (float)layout.OFFSET_Y + y * layout.CELL + 3);
                        window.draw(f);
                    }
                }
            }
        }

        // SFML: рисуем UI элементы
        window.draw(ui.restartBtn);
        window.draw(ui.restartText);
        window.draw(ui.menuBtn);
        window.draw(ui.menuText);
        window.draw(ui.status);
        window.draw(ui.minesIndicator);
        window.draw(ui.timerText);

        // SFML: показать кадр
        window.display();
    }
};

// INPUT CONTROLLER (SFML events) — здесь используется sf::Event

enum class AppActionType { None, Restart, BackToMenu, Quit };
struct AppAction { AppActionType type = AppActionType::None; };

class IInputController {
public:
    virtual ~IInputController() = default;
    virtual AppAction handleEvent(sf::RenderWindow& window, const sf::Event& e,
                                 Game& game, const Layout& layout, UiWidgets& ui) = 0;
};

class SfmlInputController final : public IInputController {
public:
    AppAction handleEvent(sf::RenderWindow& window, const sf::Event& e,
                          Game& game, const Layout& layout, UiWidgets& ui) override
    {
        // SFML: событие закрытия окна
        if (e.type == sf::Event::Closed) {
            window.close();
            return {AppActionType::Quit};
        }

        // SFML: нас интересуют только клики мыши
        if (e.type != sf::Event::MouseButtonPressed) return {};

        // SFML: получаем координаты клика мыши
        int mx = e.mouseButton.x;
        int my = e.mouseButton.y;

        // SFML: обработка UI кнопок через getGlobalBounds().contains(...)
        if (ui.restartBtn.getGlobalBounds().contains((float)mx, (float)my))
            return {AppActionType::Restart};

        if (ui.menuBtn.getGlobalBounds().contains((float)mx, (float)my))
            return {AppActionType::BackToMenu};

        // Клик вне поля (выше)
        if (my <= layout.OFFSET_Y) return {};

        // Клик левее/правее поля
        if (mx < layout.XOFFSET || mx >= layout.XOFFSET + layout.boardWidthPx) return {};

        // Переводим пиксели -> координаты клетки
        int x = (mx - layout.XOFFSET) / layout.CELL;
        int y = (my - layout.OFFSET_Y) / layout.CELL;

        if (x < 0 || x >= game.W || y < 0 || y >= game.H) return {};

        // PATTERN State: не if-else на клетку, а делегирование поведению state
        if (e.mouseButton.button == sf::Mouse::Left)  game.leftClickCell(x, y);
        if (e.mouseButton.button == sf::Mouse::Right) game.rightClickCell(x, y);

        return {};
    }
};

// MENU SCREEN (SFML UI)

class IMenuScreen {
public:
    virtual ~IMenuScreen() = default;
    virtual int run(sf::RenderWindow& window) = 0;
};

class SfmlMenuScreen final : public IMenuScreen {
    sf::Font& font;
    const ITheme& theme;
public:
    SfmlMenuScreen(sf::Font& f, const ITheme& t) : font(f), theme(t) {}

    int run(sf::RenderWindow &window) override {
        // SFML: создаём текст и кнопки меню
        sf::Text title("Select Difficulty", font, 40);
        title.setFillColor(sf::Color::Black);
        title.setPosition(120, 50);

        sf::RectangleShape easyBtn(sf::Vector2f(150, 50));
        easyBtn.setFillColor(sf::Color(150, 250, 150));
        easyBtn.setPosition(200, 150);
        sf::Text easyText("Easy", font, 30);
        easyText.setPosition(250, 155);
        easyText.setFillColor(sf::Color::Black);

        sf::RectangleShape normalBtn(sf::Vector2f(150, 50));
        normalBtn.setFillColor(sf::Color(250, 250, 150));
        normalBtn.setPosition(200, 250);
        sf::Text normalText("Normal", font, 30);
        normalText.setPosition(235, 255);
        normalText.setFillColor(sf::Color::Black);

        sf::RectangleShape hardBtn(sf::Vector2f(150, 50));
        hardBtn.setFillColor(sf::Color(250, 150, 150));
        hardBtn.setPosition(200, 350);
        sf::Text hardText("Hard", font, 30);
        hardText.setPosition(260, 355);
        hardText.setFillColor(sf::Color::Black);

        while (window.isOpen()) {
            sf::Event e;
            while (window.pollEvent(e)) {
                if (e.type == sf::Event::Closed) return 0;

                // SFML: клик мыши по кнопкам меню
                if (e.type == sf::Event::MouseButtonPressed) {
                    int mx = e.mouseButton.x;
                    int my = e.mouseButton.y;

                    if (easyBtn.getGlobalBounds().contains((float)mx, (float)my)) return 1;
                    if (normalBtn.getGlobalBounds().contains((float)mx, (float)my)) return 2;
                    if (hardBtn.getGlobalBounds().contains((float)mx, (float)my)) return 3;
                }
            }

            // SFML: рисуем меню
            window.clear(theme.bgColor());
            window.draw(title);
            window.draw(easyBtn);   window.draw(easyText);
            window.draw(normalBtn); window.draw(normalText);
            window.draw(hardBtn);   window.draw(hardText);
            window.display();
        }
        return 0;
    }
};

// Factory: create game by difficulty

static Game makeGameByDifficulty(int choice) {
    int W, H, MINES;
    switch(choice) {
        case 1: W=10; H=10; MINES=10; break;
        case 2: W=14; H=14; MINES=20; break;
        case 3: W=20; H=20; MINES=40; break;
        default: W=10; H=10; MINES=10; break;
    }

    // Здесь мы собираем игру:

    return Game(
        W, H, MINES,
        std::make_unique<DefaultCellFactory>(),
        std::make_unique<DefaultBoardGenerator>()
    );
}

// MAIN (SFML entry point)

int main() {
    sf::Font font;

    if (!font.loadFromFile("C:\\Windows\\Fonts\\arial.ttf")) {
        printf("Не удалось загрузить шрифт!\n");
        return -1;
    }

    Layout layout;

    // SFML: создаём окно
    sf::RenderWindow window(sf::VideoMode(layout.WINDOW_W, layout.WINDOW_H), "Minesweeper");

    // ThemeFactory: создаём тему (цвета/размеры) через фабрику
    auto theme = ThemeFactory::makeDefault();

    // SFML: меню выбора сложности
    SfmlMenuScreen menu(font, *theme);
    int choice = menu.run(window);
    if (choice == 0) return 0;

    // Создаём игру выбранной сложности
    Game game = makeGameByDifficulty(choice);
    layout.recompute(game);

    // SFML: создаём UI элементы (кнопки и тексты)
    UiWidgets ui;

    ui.status = sf::Text("", font, theme->hudTitleSize());
    ui.status.setFillColor(theme->statusColor());
    ui.status.setPosition(180, 5);

    ui.restartBtn = sf::RectangleShape(sf::Vector2f(150, 40));
    ui.restartBtn.setFillColor(sf::Color(200, 200, 200));
    ui.restartBtn.setPosition(600, 10);

    ui.restartText = sf::Text("Restart", font, 20);
    ui.restartText.setFillColor(sf::Color::Black);
    ui.restartText.setPosition(630, 15);

    ui.menuBtn = sf::RectangleShape(sf::Vector2f(150, 40));
    ui.menuBtn.setFillColor(sf::Color(200, 200, 200));
    ui.menuBtn.setPosition(440, 10);

    ui.menuText = sf::Text("Menu", font, 20);
    ui.menuText.setFillColor(sf::Color::Black);
    ui.menuText.setPosition(485, 15);

    ui.minesIndicator = sf::Text("", font, theme->hudSmallSize());
    ui.minesIndicator.setFillColor(sf::Color::Black);
    ui.minesIndicator.setPosition(440, 60);

    ui.timerText = sf::Text("", font, theme->hudSmallSize());
    ui.timerText.setFillColor(sf::Color::Black);
    ui.timerText.setPosition(440, 82);

    // Создаём рендерер и контроллер ввода:
    SfmlRenderer renderer(font, *theme);// работает с SFML draw()
    SfmlInputController input;// работает с SFML events

    // SFML: clock для dt (дельта времени на кадр)
    sf::Clock frameClock;

    while (window.isOpen()) {
        float dt = frameClock.restart().asSeconds();

        // Логика игры обновляется отдельно от отрисовки
        game.update(dt);

        // SFML: обработка очереди событий
        sf::Event e;
        while (window.pollEvent(e)) {
            AppAction action = input.handleEvent(window, e, game, layout, ui);

            // Реакции приложения на кнопки
            if (action.type == AppActionType::Restart) {
                game.resetField();
                layout.recompute(game);
            } else if (action.type == AppActionType::BackToMenu) {
                int newChoice = menu.run(window);
                if (newChoice == 0) return 0;
                game = makeGameByDifficulty(newChoice);
                layout.recompute(game);
            }
        }

        // SFML: рисуем кадр
        renderer.render(window, game, layout, ui);
    }

    return 0;
}
