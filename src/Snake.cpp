#include <Snake.hpp>
#include <cassert>
#include <queue>
#include <fstream>
#include <cmath>
#include <iostream>
#include <NeuralNetwork.hpp>
#include <ByteStream.hpp>
#include <Visualizer.hpp>

using namespace zeuron;
using namespace bs;
using namespace snake;

static const auto cellSize = 20;
static const auto cells = 20;
auto boardWidth = cells * cellSize;
auto boardHeight = cells * cellSize;
bool trainingAI = false;

std::shared_ptr<NeuralNetwork> loadOrCreateAINetwork();
void saveAINetwork();
std::mutex aiNetworkMutex;
std::shared_ptr<NeuralNetwork> aiNetwork;

int main()
{
  aiNetwork = loadOrCreateAINetwork();
  Visualizer visualizer(*aiNetwork, 640, 480);
  SnakeGame game((boardWidth * 2) + (boardWidth / 2), boardHeight + (boardHeight / 2));
  game.awaitWindowThread();
  visualizer.close();
  visualizer.awaitWindowThread();
  saveAINetwork();
};

ButtonEntity::ButtonEntity(anex::IGame& game,
													 const char* text,
													 const int& x,
													 const int& y,
													 const int& width,
													 const int& height,
													 const int& borderWidth,
													 const int& padding,
													 const bool& selected,
													 const std::function<void()>& onEnter):
	IEntity(game),
	text(text),
	x(x),
	y(y),
	width(width),
	height(height),
	borderWidth(borderWidth),
	padding(padding),
	selected(selected),
	scale((height / 2 - (padding * 2) - (borderWidth * 2)) / 5),
	textBounds(fenster_text_bounds(text, scale)),
	onEnter(onEnter)
{
};

void ButtonEntity::render()
{
	auto &fensterGame = (FensterGame &)game;
	uint32_t borderColor = selected ? 0x00999999 : 0x00555555;
	uint32_t bgColor = selected ? 0x00222222 : 0x00000000;
	fenster_rect(fensterGame.f, x, y, width, height, borderColor);
	fenster_rect(fensterGame.f, x + borderWidth, y + borderWidth, width - borderWidth * 2, height - borderWidth * 2,
							 bgColor);
	fenster_text(fensterGame.f, x + width / 2 - std::get<0>(textBounds) / 2, y + height / 2 - std::get<1>(textBounds) / 2,
							 text, scale, 0x00ffffff);
};

SnakeGame::SnakeGame(const int& windowWidth, const int& windowHeight):
	FensterGame(windowWidth, windowHeight)
{
	setIScene(std::make_shared<MainMenuScene>(*this));
	escKeyId = addKeyHandler(27, std::bind(&SnakeGame::onEscape, this, std::placeholders::_1));
};

void SnakeGame::onEscape(const bool& pressed)
{
	if (pressed)
	{
		close();
	}
};

bool iPoint2D::operator==(const iPoint2D &other) const
{
  return x == other.x && y == other.y;
};

Snake::Snake(anex::IGame &game, GameBoard &gameBoard):
  IEntity(game),
  gameBoard(gameBoard)
{
  reset();
};

void Snake::render()
{
  auto &fensterGame = (FensterGame &)game;
  bool firstSegment = true;
  for (const auto &segment : segments)
  {
    int renderX = gameBoard.x + (segment.x - cells / 2) * gameBoard.cellSize;
    int renderY = gameBoard.y + (segment.y - cells / 2) * gameBoard.cellSize;
    fenster_rect(fensterGame.f, renderX, renderY, gameBoard.cellSize, gameBoard.cellSize, firstSegment ? 0x0000FF00 : 0x0000FF99);
    if (firstSegment)
      firstSegment = false;
  }
};

void Snake::update()
{
  if (!gameBoard.gameOver)
  {
    // Get the current head position
    auto head = segments.front();
    switch (direction)
    {
      case Direction::Up:    head.y--; break;
      case Direction::Down:  head.y++; break;
      case Direction::Left:  head.x--; break;
      case Direction::Right: head.x++; break;
    }
    // Check collisions
    if (head.x < 0)
    {
      head.x = gameBoard.width / gameBoard.cellSize - 1;
    }
    else if (head.x >= gameBoard.width / cellSize)
    {
      head.x = 0;
    }
    else if (head.y < 0)
    {
      head.y = gameBoard.height / gameBoard.cellSize - 1;
    }
    else if (head.y >= gameBoard.height / cellSize)
    {
      head.y = 0;
    }
    else if (std::find(segments.begin(), segments.end(), head) != segments.end())
    {
      gameBoard.gameOver = true;
      if (trainingAI)
      {
        // std::this_thread::sleep_for(std::chrono::seconds(2));
        reset();
        gameBoard.gameOver = false;
        gameBoard.score = 0;
      }
      else
      {
        return;
      }
    }
    // Move the snake
    segments.push_front(head);
    if (head == gameBoard.fruit)
    {
      gameBoard.score++;
      gameBoard.setFruitToRandom();
    }
    else
    {
      segments.pop_back();
    }
    // std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
};

void Snake::onUpKey(const bool &pressed)
{
  if (!pressed || direction == Direction::Down)
  {
    return;
  }
  direction = Direction::Up;
};

void Snake::onDownKey(const bool &pressed)
{
  if (!pressed || direction == Direction::Up)
  {
    return;
  }
  direction = Direction::Down;
};

void Snake::onLeftKey(const bool &pressed)
{
  if (!pressed || direction == Direction::Right)
  {
    return;
  }
  direction = Direction::Left;
};

void Snake::onRightKey(const bool &pressed)
{
  if (!pressed || direction == Direction::Left)
  {
    return;
  }
  direction = Direction::Right;
};

void Snake::reset()
{
  segments.clear();
  iPoint2D head{cells / 2, cells / 2};
  segments.push_back(head);
  segments.push_back({head.x - 1, head.y});
  direction = Direction::Right;
};

PlayerSnake::PlayerSnake(anex::IGame &game, GameBoard &gameBoard):
  Snake(game, gameBoard)
{
  upKeyId = game.addKeyHandler(
    gameBoard.useKeys == GameBoard::UseKeys::WSAD ? 87 : 17,
    std::bind(&Snake::onUpKey, this, std::placeholders::_1)
  );
  downKeyId = game.addKeyHandler(
    gameBoard.useKeys == GameBoard::UseKeys::WSAD ? 83 : 18,
    std::bind(&Snake::onDownKey, this, std::placeholders::_1)
    );
  leftKeyId = game.addKeyHandler(
    gameBoard.useKeys == GameBoard::UseKeys::WSAD ? 65 : 20,
    std::bind(&Snake::onLeftKey, this, std::placeholders::_1)
  );
  rightKeyId = game.addKeyHandler(
    gameBoard.useKeys == GameBoard::UseKeys::WSAD ? 68 : 19,
    std::bind(&Snake::onRightKey, this, std::placeholders::_1)
  );
};

PlayerSnake::~PlayerSnake()
{
  game.removeKeyHandler(gameBoard.useKeys == GameBoard::UseKeys::WSAD ? 87 : 17, upKeyId);
  game.removeKeyHandler(gameBoard.useKeys == GameBoard::UseKeys::WSAD ? 83 : 18, downKeyId);
  game.removeKeyHandler(gameBoard.useKeys == GameBoard::UseKeys::WSAD ? 65 : 20, leftKeyId);
  game.removeKeyHandler(gameBoard.useKeys == GameBoard::UseKeys::WSAD ? 68 : 19, rightKeyId);
};

AISnake::AISnake(anex::IGame &game, GameBoard &gameBoard):
  Snake(game, gameBoard)
{};

long double distance(const long double& a, const long double& b)
{
  return std::abs(a - b);
}

long double distance(const std::pair<long double, long double>& point1,
                     const std::pair<long double, long double>& point2)
{
  long double dx = point1.first - point2.first;
  long double dy = point1.second - point2.second;
  return std::sqrt(dx * dx + dy * dy);
}

void AISnake::activation()
{
  auto& aiNetworkRef = *aiNetwork;
  auto gridHeight = gameBoard.height / cellSize;
  auto gridWidth = gameBoard.width / cellSize;
  auto &segments = this->segments;
  auto head = segments.front();
  auto &fruit = gameBoard.fruit;
  auto DistanceToWallUp = computeDistanceToWallUp(head, gridHeight);
  auto DistanceToWallDown = computeDistanceToWallDown(head, gridHeight);
  auto DistanceToWallLeft = computeDistanceToWallLeft(head, gridWidth);
  auto DistanceToWallRight = computeDistanceToWallRight(head, gridWidth);
  auto DistanceToSnakeUp = computeDistanceToSnakeUp(head, segments);
  auto DistanceToSnakeDown = computeDistanceToSnakeDown(head, segments, gridHeight);
  auto DistanceToSnakeLeft = computeDistanceToSnakeLeft(head, segments);
  auto DistanceToSnakeRight = computeDistanceToSnakeRight(head, segments, gridWidth);
  auto RelativeFruitX = computeRelativeFruitX(head, fruit, gridWidth);
  auto RelativeFruitY = computeRelativeFruitY(head, fruit, gridHeight);
  auto DirectionX = computeDirectionX(direction);
  auto DirectionY = computeDirectionY(direction);
  auto SnakeLength = computeSnakeLength(segments);

  std::vector<long double> input({
    DistanceToWallUp, DistanceToWallDown, DistanceToWallLeft, DistanceToWallRight,
    DistanceToSnakeUp, DistanceToSnakeDown, DistanceToSnakeLeft, DistanceToSnakeRight,
    RelativeFruitX, RelativeFruitY, DirectionX, DirectionY, SnakeLength
  });

  aiNetworkRef.feedforward(input);
  auto outputs = aiNetworkRef.getOutputs();

  // Initial move decisions based on neural network output
  if (distance(outputs[0], 1) <= 0.05)
  {
    onUpKey(true);
  }
  else if (distance(outputs[1], 1) <= 0.05)
  {
    onDownKey(true);
  }
  else if (distance(outputs[2], 1) <= 0.05)
  {
    onLeftKey(true);
  }
  else if (distance(outputs[3], 1) <= 0.05)
  {
    onRightKey(true);
  }

  auto path = gameBoard.aStar(head, fruit);
  std::vector<long double> expectedOutputs(4, 0.0); // Initialize to 0 for all directions

  // If there is no path, stop the snake from moving
  if (path.empty())
  {
    goto _backprop;
  }
  {
    // Analyzing up to 3 steps ahead in the path
    auto bestDirection = -1;
    long double bestScore = -std::numeric_limits<long double>::infinity();
    for (size_t i = 1; i <= std::min<size_t>(5, path.size() - 1); ++i)
    {
      auto nextMove = path[i];
      auto deltaX = nextMove.x - head.x;
      auto deltaY = nextMove.y - head.y;

      // Calculate score based on proximity to fruit
      long double score = 0.0;
      if (nextMove == fruit)
      {
        score += 10.0; // Reward for moving towards the fruit
      }
      else
      {
        score += (gridWidth - std::abs(deltaX)) + (gridHeight - std::abs(deltaY)); // Reward for staying within the grid
      }

      // Determine the direction based on the step
      int directionIndex = -1;
      if (deltaX < 0)
      {
        directionIndex = 2; // Left
      }
      else if (deltaX > 0)
      {
        directionIndex = 3; // Right
      }
      else if (deltaY < 0)
      {
        directionIndex = 0; // Up
      }
      else if (deltaY > 0)
      {
        directionIndex = 1; // Down
      }

      // If this step has a higher score, select it as the best move
      if (score > bestScore)
      {
        bestScore = score;
        bestDirection = directionIndex;
      }
    }

    // Assign the best direction based on the analysis of the first 3 steps ahead
    if (bestDirection != -1)
    {
      expectedOutputs[bestDirection] = 1.0;
    }
  }

_backprop:
  aiNetworkRef.backpropagate(expectedOutputs);
};

bool AISnake::isCollisionAhead(const iPoint2D& head, Direction direction)
{
  // Check if there is a collision ahead based on the direction
  iPoint2D nextPos = head;
  switch (direction)
  {
  case Direction::Up:
    nextPos.y -= 1;
    break;
  case Direction::Down:
    nextPos.y += 1;
    break;
  case Direction::Left:
    nextPos.x -= 1;
    break;
  case Direction::Right:
    nextPos.x += 1;
    break;
  }

  // Check if the new position is a wall or part of the snake's body
  if (nextPos.x < 0 || nextPos.x >= gameBoard.width / cellSize ||
      nextPos.y < 0 || nextPos.y >= gameBoard.height / cellSize)
  {
    return true; // Wall collision
  }

  for (const auto& segment : segments)
  {
    if (nextPos == segment)
    {
      return true; // Snake body collision
    }
  }

  return false; // No collision
}

// Function to compute distances to walls
long double AISnake::computeDistanceToWallUp(const iPoint2D& head, const int &gridHeight) {
    return static_cast<long double>(head.y);
};

long double AISnake::computeDistanceToWallDown(const iPoint2D& head, const int &gridHeight) {
    return static_cast<long double>(gridHeight - head.y - 1);
};

long double AISnake::computeDistanceToWallLeft(const iPoint2D& head, const int &gridWidth) {
    return static_cast<long double>(head.x);
};

long double AISnake::computeDistanceToWallRight(const iPoint2D& head, const int &gridWidth) {
    return static_cast<long double>(gridWidth - head.x - 1);
};

// Function to compute distances to snake segments
long double AISnake::computeDistanceToSnakeUp(const iPoint2D& head, const std::deque<iPoint2D>& segments) {
    for (int y = head.y - 1; y >= 0; --y) {
        if (std::find(segments.begin(), segments.end(), iPoint2D{head.x, y}) != segments.end()) {
            return static_cast<long double>(head.y - y);
        }
    }
    return static_cast<long double>(head.y + 1);
};

long double AISnake::computeDistanceToSnakeDown(const iPoint2D& head, const std::deque<iPoint2D>& segments, const int &gridHeight) {
    for (int y = head.y + 1; y < gridHeight; ++y) {
        if (std::find(segments.begin(), segments.end(), iPoint2D{head.x, y}) != segments.end()) {
            return static_cast<long double>(y - head.y);
        }
    }
    return static_cast<long double>(gridHeight - head.y);
};

long double AISnake::computeDistanceToSnakeLeft(const iPoint2D& head, const std::deque<iPoint2D>& segments) {
    for (int x = head.x - 1; x >= 0; --x) {
        if (std::find(segments.begin(), segments.end(), iPoint2D{x, head.y}) != segments.end()) {
            return static_cast<long double>(head.x - x);
        }
    }
    return static_cast<long double>(head.x + 1);
};

long double AISnake::computeDistanceToSnakeRight(const iPoint2D& head, const std::deque<iPoint2D>& segments, const int &gridWidth) {
    for (int x = head.x + 1; x < gridWidth; ++x) {
        if (std::find(segments.begin(), segments.end(), iPoint2D{x, head.y}) != segments.end()) {
            return static_cast<long double>(x - head.x);
        }
    }
    return static_cast<long double>(gridWidth - head.x);
};

// Function to compute relative position of the fruit
long double AISnake::computeRelativeFruitX(const iPoint2D& head, const iPoint2D& fruit, const int &gridWidth) {
    long double deltaX = static_cast<long double>(fruit.x - head.x);
    if (deltaX > gridWidth / 2.0) deltaX -= gridWidth;
    if (deltaX < -gridWidth / 2.0) deltaX += gridWidth;
    return deltaX;
};

long double AISnake::computeRelativeFruitY(const iPoint2D& head, const iPoint2D& fruit, const int &gridHeight) {
    long double deltaY = static_cast<long double>(fruit.y - head.y);
    if (deltaY > gridHeight / 2.0) deltaY -= gridHeight;
    if (deltaY < -gridHeight / 2.0) deltaY += gridHeight;
    return deltaY;
};

// Function to compute direction as two separate values
long double AISnake::computeDirectionX(const Direction &direction) {
    switch (direction) {
        case Direction::Left: return -1.0;
        case Direction::Right: return 1.0;
        default: return 0.0;
    }
};

long double AISnake::computeDirectionY(const Direction &direction) {
    switch (direction) {
        case Direction::Up: return -1.0;
        case Direction::Down: return 1.0;
        default: return 0.0;
    }
};

// Function to compute length of the snake
long double AISnake::computeSnakeLength(const std::deque<iPoint2D>& segments) {
    return static_cast<long double>(segments.size());
};

GameBoard::GameBoard(anex::IGame &game,
                     const int &x,
                     const int &y,
                     const int &width,
                     const int &height,
                     const int &cellSize,
                     const UseKeys &useKeys,
                     const bool &isAI):
  IEntity(game),
  x(x),
  y(y),
  width(width),
  height(height),
  cellSize(cellSize),
  useKeys(useKeys),
  isAI(isAI)
{
  snake = isAI ? std::dynamic_pointer_cast<Snake>(std::make_shared<AISnake>(game, *this)) :
                 std::dynamic_pointer_cast<Snake>(std::make_shared<PlayerSnake>(game, *this));
  setFruitToRandom();
};

void GameBoard::render()
{
  if (isAI)
  {
    auto aiSnake = std::dynamic_pointer_cast<AISnake>(snake);
    aiSnake->activation();
  }
  snake->update();
  auto &fensterGame = (FensterGame &)game;
  int left = x - (width / 2);
  int top = y - (height / 2);
  // Render grid using lines
  for (int i = 0; i <= (width / cellSize); ++i)
  {
    int lineX = left + i * cellSize;
    fenster_line(fensterGame.f, lineX, top, lineX, top + height - 1, 0x808080FF);
  }
  for (int j = 0; j <= (height / cellSize); ++j)
  {
    int lineY = top + j * cellSize;
    fenster_line(fensterGame.f, left, lineY, left + width - 1, lineY, 0x808080FF);
  }
  // Render optimal path
  {
    auto path = aStar(snake->segments.front(), fruit);
    for (auto &pathCell : path)
    {
      int renderX = x + (pathCell.x - cells / 2) * cellSize;
      int renderY = y + (pathCell.y - cells / 2) * cellSize;
      fenster_rect(fensterGame.f, renderX, renderY, cellSize, cellSize, 0x00FF0000);
    }
  }
  // Render snake
  snake->render();
  // Render score and gameover text
  static const auto textScale = 5;
  static const auto textHeight = 5 * textScale;
  auto text = "Score: " + std::to_string(score) + (gameOver ? " Game Over" : "");
  fenster_text(fensterGame.f, left, top - textHeight, text.c_str(), textScale, 0x00ffffff);
  // Render fruit
  int fruitRenderX = left + fruit.x * cellSize;
  int fruitRenderY = top + fruit.y * cellSize;
  fenster_rect(fensterGame.f, fruitRenderX, fruitRenderY, cellSize, cellSize, 0xFF0000FF);
};

void GameBoard::setFruitToRandom()
{
  // Create a 2D boolean array to mark valid positions
  std::vector<std::vector<bool>> validPositions(height / cellSize, std::vector<bool>(width / cellSize, true));

  // Mark snake segments as invalid
  {
    for (const auto& segment : snake->segments) {
      validPositions[segment.x][segment.y] = false;
    }
  }

  // Collect all valid positions
  std::vector<iPoint2D> possiblePositions;
  for (int x = 0; x < width / cellSize; ++x) {
    for (int y = 0; y < height / cellSize; ++y) {
      if (validPositions[x][y]) {
        possiblePositions.push_back({x, y});
      }
    }
  }

  // Randomly pick a valid position for the fruit
  if (!possiblePositions.empty()) {
    int randomIndex = rand() % possiblePositions.size();
    fruit = possiblePositions[randomIndex];
  }
};

std::vector<std::vector<bool>> GameBoard::getGrid() const
{
  std::vector<std::vector<bool>> grid;
  grid.resize(height / cellSize, std::vector<bool>(width / cellSize, false));
  auto &snake = *this->snake;
  for (auto &segment : snake.segments)
  {
    grid[segment.x][segment.y] = true;
  }
  return grid;
};

size_t iPointHash2D::operator()(const iPoint2D& p) const
{
  return std::hash<int>()(p.x) ^ std::hash<int>()(p.y);
};

int Node::fCost() const
{
  return gCost + hCost; // Total cost
};

// Compare Nodes for priority queue (min-heap)
bool Node::operator<(const Node& other) const
{
  return fCost() > other.fCost(); // Higher cost -> lower priority
};

// Check if a point is within bounds and not blocked
bool isValid(int x, int y, const std::vector<std::vector<bool>>& grid)
{
  return x >= 0 && x < grid.size() && y >= 0 && y < grid[0].size() && grid[x][y] == false;
};

// Manhattan distance heuristic
int manhattanDistance(const iPoint2D& a, const iPoint2D& b)
{
  return abs(a.x - b.x) + abs(a.y - b.y);
};

std::vector<iPoint2D> GameBoard::aStar(const iPoint2D &start, const iPoint2D &target) const
{
  // Directions for movement: up, right, down, left
  std::vector<iPoint2D> directions = {{-1, 0}, {0, 1}, {1, 0}, {0, -1}};

  // Priority queue for open list (min-heap based on fCost)
  std::priority_queue<Node> openList;

  // Maps to store gCost and parent of each point
  std::unordered_map<iPoint2D, int, iPointHash2D> gCost;
  std::unordered_map<iPoint2D, iPoint2D, iPointHash2D> parent;

  // Initialize start node
  gCost[start] = 0;
  openList.push({start, 0, manhattanDistance(start, target)});

  auto grid = getGrid();
  int gridWidth = width / cellSize;
  int gridHeight = height / cellSize;

  while (!openList.empty())
  {
    Node current = openList.top();
    openList.pop();

    // If we reached the target, reconstruct the path
    if (current.point == target)
    {
      std::vector<iPoint2D> path;
      iPoint2D p = target;
      while (p != start) {
        path.push_back(p);
        p = parent[p];
      }
      path.push_back(start);
      std::reverse(path.begin(), path.end());
      return path;
    }

    // Explore neighbors
    for (const iPoint2D& dir : directions)
    {
      iPoint2D neighbor = {current.point.x + dir.x, current.point.y + dir.y};

      // Apply wrap-around logic
      if (neighbor.x < 0)
      {
        neighbor.x = gridWidth - 1;
      }
      else if (neighbor.x >= gridWidth)
      {
        neighbor.x = 0;
      }

      if (neighbor.y < 0)
      {
        neighbor.y = gridHeight - 1;
      }
      else if (neighbor.y >= gridHeight)
      {
        neighbor.y = 0;
      }

      // Check if the neighbor is valid
      if (isValid(neighbor.x, neighbor.y, grid))
      {
        int tentativeGCost = gCost[current.point] + 1; // Cost to move to neighbor

        if (gCost.find(neighbor) == gCost.end() || tentativeGCost < gCost[neighbor])
        {
          // Update gCost and parent
          gCost[neighbor] = tentativeGCost;
          parent[neighbor] = current.point;

          // Add neighbor to open list
          openList.push({neighbor, tentativeGCost, manhattanDistance(neighbor, target)});
        }
      }
    }
  }

  // Return empty path if no path exists
  return {};
};

MainMenuScene::MainMenuScene(anex::IGame& game):
  IScene(game),
  borderWidth(4),
  padding(4),
  playerVsAIButton(std::make_shared<ButtonEntity>(game, "Player vs AI", 0, 0, int(game.windowWidth / 1.5),
                                                  game.windowHeight / 6, borderWidth, padding, true,
                                                  std::bind(&MainMenuScene::onPlayerVsAIEnter, this))),
  trainAIButton(std::make_shared<ButtonEntity>(game, "Train AI", 0, 0, int(game.windowWidth / 1.5),
                                               game.windowHeight / 6, borderWidth, padding, false,
                                               std::bind(&MainMenuScene::onTrainAIEnter, this))),
  playerVsPlayerButton(std::make_shared<ButtonEntity>(game, "Player vs Player", 0, 0,
                                                      int(game.windowWidth / 1.5), game.windowHeight / 6,
                                                      borderWidth, padding, false,
                                                      std::bind(&MainMenuScene::onPlayerVsPlayerEnter, this))),
  singleplayerButton(std::make_shared<ButtonEntity>(game, "Singleplayer", 0, 0,
                                                      int(game.windowWidth / 1.5), game.windowHeight / 6,
                                                      borderWidth, padding, false,
                                                      std::bind(&MainMenuScene::onSingleplayerEnter, this))),
  exitButton(std::make_shared<ButtonEntity>(game, "Exit", 0, 0, int(game.windowWidth / 1.5),
                                            game.windowHeight / 6, borderWidth, padding, false,
                                            std::bind(&MainMenuScene::onExitEnter, this))),
  buttonsList({playerVsAIButton, trainAIButton, playerVsPlayerButton, singleplayerButton, exitButton})
{
  addEntity(playerVsAIButton);
  addEntity(trainAIButton);
  addEntity(playerVsPlayerButton);
  addEntity(singleplayerButton);
  addEntity(exitButton);
  positionButtons();
  upKeyId = game.addKeyHandler(17, std::bind(&MainMenuScene::onUpKey, this, std::placeholders::_1));
  downKeyId = game.addKeyHandler(18, std::bind(&MainMenuScene::onDownKey, this, std::placeholders::_1));
  enterKeyId = game.addKeyHandler(10, std::bind(&MainMenuScene::onEnterKey, this, std::placeholders::_1));
};

MainMenuScene::~MainMenuScene()
{
  game.removeKeyHandler(17, upKeyId);
  game.removeKeyHandler(18, downKeyId);
  game.removeKeyHandler(10, enterKeyId);
};

void MainMenuScene::positionButtons()
{
  int buttonsTotalX = 0, buttonsTotalY = 0;
  auto buttonsListSize = buttonsList.size();
  auto buttonsListData = buttonsList.data();
  buttonsTotalX += buttonsListData[0]->width;
  for (unsigned int buttonIndex = 0; buttonIndex < buttonsListSize; ++buttonIndex)
  {
    auto& button = buttonsListData[buttonIndex];
    buttonsTotalY += button->height;
    if (buttonIndex < buttonsListSize - 1)
    {
      buttonsTotalY += 2;
    }
  }
  int placementX = game.windowWidth / 2 - buttonsTotalX / 2;
  int placementY = game.windowHeight / 2 - buttonsTotalY / 2;
  for (unsigned int buttonIndex = 0; buttonIndex < buttonsListSize; ++buttonIndex)
  {
    auto& button = buttonsListData[buttonIndex];
    button->x = placementX;
    button->y = placementY;
    placementY += button->height;
    if (buttonIndex < buttonsListSize - 1)
    {
      placementY += 2;
    }
  }
};

void MainMenuScene::onUpKey(const bool& pressed)
{
  if (pressed)
  {
    auto buttonsListSize = buttonsList.size();
    auto buttonsListData = buttonsList.data();
    for (int i = 0; i < buttonsListSize; ++i)
    {
      auto& button = buttonsList[i];
      if (button->selected)
      {
        button->selected = false;
        auto prevI = i - 1;
        if (prevI >= 0)
        {
          buttonsListData[prevI]->selected = true;
        }
        else
        {
          buttonsListData[buttonsListSize - 1]->selected = true;
        }
        break;
      }
    }
  }
};

void MainMenuScene::onDownKey(const bool& pressed)
{
  if (pressed)
  {
    auto buttonsListSize = buttonsList.size();
    auto buttonsListData = buttonsList.data();
    for (int i = 0; i < buttonsListSize; ++i)
    {
      auto& button = buttonsList[i];
      if (button->selected)
      {
        button->selected = false;
        auto nextI = i + 1;
        if (nextI < buttonsListSize)
        {
          buttonsListData[nextI]->selected = true;
        }
        else
        {
          buttonsListData[0]->selected = true;
        }
        break;
      }
    }
  }
};

void MainMenuScene::onEnterKey(const bool& pressed)
{
  if (pressed)
  {
    auto buttonsListSize = buttonsList.size();
    auto buttonsListData = buttonsList.data();
    for (int i = 0; i < buttonsListSize; ++i)
    {
      auto& button = buttonsList[i];
      if (button->selected)
      {
        button->onEnter();
        break;
      }
    }
  }
};

void MainMenuScene::onPlayerVsAIEnter()
{
  game.setIScene(std::make_shared<SnakeScene>(
    game, 2, true
  ));
};

void MainMenuScene::onTrainAIEnter()
{
  trainingAI = true;
  game.setIScene(std::make_shared<SnakeScene>(
    game, 2, true, true
  ));
};

void MainMenuScene::onPlayerVsPlayerEnter()
{
  game.setIScene(std::make_shared<SnakeScene>(
    game, 2
  ));
};

void MainMenuScene::onSingleplayerEnter()
{
  game.setIScene(std::make_shared<SnakeScene>(
    game, 1
  ));
};

void MainMenuScene::onExitEnter()
{
  game.close();
};

SnakeScene::SnakeScene(anex::IGame &game, const unsigned int& boardsCount, const bool &player1IsAI, const bool &player2IsAI):
  IScene(game)
{
  assert(boardsCount == 1 || boardsCount == 2);
  auto boardX = game.windowWidth / 2 - ((boardWidth / 2 + boardWidth / 8) * (boardsCount > 1 ? 1 : 0));
  auto boardY = game.windowHeight / 2;

  for (unsigned int i = 0; i < boardsCount; ++i)
  {
    auto isAI = i == 0 ? player1IsAI : player2IsAI;
    auto gameBoardIter = gameBoards.insert(gameBoards.end(), std::make_shared<GameBoard>(
      game, boardX, boardY, boardWidth, boardHeight,
      cellSize, i == 0 && boardsCount == 2 ? GameBoard::UseKeys::WSAD : GameBoard::UseKeys::UpDownLeftRight,
      isAI));
    auto& gameBoard = **gameBoardIter;
    boardX += boardWidth + boardWidth / 4;
  }
  for (auto &gameBoard : gameBoards)
  {
    addEntity(gameBoard);
  }
};

std::pair<std::shared_ptr<char>, unsigned long> readFileToBuffer(const std::string& filename)
{
  std::ifstream file(filename, std::ios::binary | std::ios::ate);
  if (!file.is_open())
  {
    throw std::ios_base::failure("Error: Unable to open file for reading.");
  }
  std::streampos fileSize = file.tellg();
  if (fileSize <= 0)
  {
    throw std::ios_base::failure("Error: File is empty or has invalid size.");
  }
  unsigned long size = static_cast<unsigned long>(fileSize);
  std::shared_ptr<char> buffer(new char[size], std::default_delete<char[]>());
  file.seekg(0, std::ios::beg);
  file.read(buffer.get(), size);
  if (!file)
  {
    throw std::ios_base::failure("Error: Reading the file failed.");
  }
  file.close();
  return std::make_pair(buffer, size);
};

void writeBufferToFile(const char* buffer, unsigned long size, const std::string& filename)
{
  std::ofstream file(filename, std::ios::binary);
  if (!file.is_open())
  {
    std::cerr << "Error: Unable to open file for writing.\n";
    return;
  }
  file.write(buffer, static_cast<std::streamsize>(size));
  if (!file)
  {
    std::cerr << "Error: Writing to the file failed.\n";
  }
  file.close();
};

std::shared_ptr<NeuralNetwork> loadOrCreateAINetwork()
{
  try
  {
    auto bytesSizePair = readFileToBuffer("snake.nrl");
    ByteStream byteStream(std::get<1>(bytesSizePair), std::get<0>(bytesSizePair));
    return std::make_shared<NeuralNetwork>(byteStream);
  }
  catch (...)
  {
    return std::make_shared<NeuralNetwork>(
      13, // Inputs: distance to walls [up, down, left, right], distance to snake segments [up, down, left, right], relative position of fruit (x, y), current direction (encoded as 2 values for direction x and y), and length of the snake
      std::vector<std::pair<NeuralNetwork::ActivationType, unsigned long>>({
        {NeuralNetwork::HardSigmoid, 32},
        {NeuralNetwork::Tanh, 24},
        {NeuralNetwork::Tanh, 16},
        {NeuralNetwork::Softplus, 12},
        {NeuralNetwork::BentIdentity, 8},
        {NeuralNetwork::HardSigmoid, 4}
      }),
      0.01 // Reduced learning rate to account for the deeper architecture
    );
  }
};

void saveAINetwork()
{
  auto nnStream = aiNetwork->serialize();
  writeBufferToFile(nnStream.bytes.get(), nnStream.bytesSize, "snake.nrl");
};