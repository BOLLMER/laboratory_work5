# UML диаграмма классов (итоговая версия)

```mermaid
classDiagram
direction TB

%% =========================
%% Game — логика игры
%% =========================
class Game {
  -int W
  -int H
  -int MINES
  -bool gameOver
  -bool win
  -bool firstClick
  -bool explosion
  -float explosionTimer
  -float timeElapsed
  -vector<vector<Cell>> field

  +Game(w:int, h:int, mines:int)
  +resetField() void
  +update(dt:float) void
  +leftClickCell(x:int, y:int) void
  +rightClickCell(x:int, y:int) void
  +countMinesAround(x:int, y:int) int
  +floodFill(x:int, y:int) void
  +triggerExplosion() void
  +checkWin() void
}

%% =========================
%% Cell — клетка поля
%% =========================
class Cell {
  -ICellContent* content
  -ICellState* state
}

Game "1" --> "many" Cell : содержит

%% =========================
%% Содержимое клетки
%% =========================
class ICellContent {
  <<interface>>
  +isMine() bool
  +number() int
}

class MineContent
class NumberContent
class EmptyContent

ICellContent <|-- MineContent
ICellContent <|-- NumberContent
ICellContent <|-- EmptyContent

Cell --> ICellContent : содержит

%% =========================
%% Состояние клетки (State)
%% =========================
class ICellState {
  <<interface>>
  +onLeftClick(game,x,y)
  +onRightClick(game,x,y)
  +isOpen() bool
  +isFlagged() bool
}

class ClosedState
class OpenedState
class FlaggedState

ICellState <|-- ClosedState
ICellState <|-- OpenedState
ICellState <|-- FlaggedState

Cell --> ICellState : содержит
ICellState --> Game : уведомляет о событиях

%% =========================
%% MinesweeperApp / main
%% =========================
class MinesweeperApp {
  -sf::RenderWindow window
  -sf::Font font
  -Game game
  -int cellSize
  -int offsetY

  +run() int
  -handleInput() void
  -renderGame() void
  -updateUI() void
}

MinesweeperApp --> Game : управляет

%% =========================
%% Внешние библиотеки SFML
%% =========================
class SFML_Window {
  <<external>>
  sf::RenderWindow
  sf::Event
  sf::Mouse
}

class SFML_Graphics {
  <<external>>
  sf::Font
  sf::Text
  sf::RectangleShape
  sf::Color
}

MinesweeperApp --> SFML_Window : использует
MinesweeperApp --> SFML_Graphics : использует
