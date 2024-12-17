#pragma once
#include <anex/modules/fenster/Fenster.hpp>
#include <deque>
#include <mutex>
using namespace anex::modules::fenster;
namespace snake
{
	struct SnakeGame;
	struct SnakeScene;
	struct ButtonEntity : anex::IEntity
	{
		const char *text;
		int x;
		int y;
		int width;
		int height;
		int borderWidth;
		int padding;
		bool selected;
		int scale;
		std::pair<int, int> textBounds;
		std::function<void()> onEnter;
		ButtonEntity(anex::IGame &game,
								 const char *text,
								 const int &x,
								 const int &y,
								 const int &width,
								 const int &height,
								 const int &borderWidth,
								 const int &padding,
								 const bool &selected,
								 const std::function<void()> &onEnter);
		void render() override;
	};
	struct SnakeGame : FensterGame
	{
		unsigned int escKeyId = 0;
		SnakeGame(const int &windowWidth, const int &windowHeight);
		void onEscape(const bool &pressed);
	};
	struct iPoint2D
	{
		int x;
		int y;
		bool operator==(const iPoint2D &other) const;
	};
	enum class Direction
	{
		None = 0,
		Up,
		Down,
		Left,
		Right
	};
	struct GameBoard;
	struct Snake : anex::IEntity
	{
		GameBoard &gameBoard;
		std::deque<iPoint2D> segments;
		Direction direction;
		std::recursive_mutex segmentsMutex;
		Snake(anex::IGame &game, GameBoard &gameBoard);
		void render() override;
		void update();
		void onUpKey(const bool &pressed);
		void onDownKey(const bool &pressed);
		void onLeftKey(const bool &pressed);
		void onRightKey(const bool &pressed);
		void reset();
	};
	struct PlayerSnake : Snake
	{
		unsigned int upKeyId = 0;
		unsigned int downKeyId = 0;
		unsigned int leftKeyId = 0;
		unsigned int rightKeyId = 0;
		PlayerSnake(anex::IGame &game, GameBoard &gameBoard);
		~PlayerSnake();
	};
	struct AISnake : Snake
	{
		SnakeScene *snakeScenePointer = 0;
		AISnake(anex::IGame &game, GameBoard &gameBoard);
		void activation();
		bool isCollisionAhead(const iPoint2D& head, Direction direction);
		// Function to compute distances to walls
		long double computeDistanceToWallUp(const iPoint2D& head, const int &gridHeight);
		long double computeDistanceToWallDown(const iPoint2D& head, const int &gridHeight);
		long double computeDistanceToWallLeft(const iPoint2D& head, const int &gridWidth);
		long double computeDistanceToWallRight(const iPoint2D& head, const int &gridWidth);
		// Function to compute distances to snake segments
		long double computeDistanceToSnakeUp(const iPoint2D& head, const std::deque<iPoint2D>& segments);
		long double computeDistanceToSnakeDown(const iPoint2D& head, const std::deque<iPoint2D>& segments, const int &gridHeight);
		long double computeDistanceToSnakeLeft(const iPoint2D& head, const std::deque<iPoint2D>& segments);
		long double computeDistanceToSnakeRight(const iPoint2D& head, const std::deque<iPoint2D>& segments, const int &gridWidth);
		// Function to compute relative position of the fruit
		long double computeRelativeFruitX(const iPoint2D& head, const iPoint2D& fruit, const int &gridWidth);
		long double computeRelativeFruitY(const iPoint2D& head, const iPoint2D& fruit, const int &gridHeight);
		// Function to compute direction as two separate values
		long double computeDirectionX(const Direction &direction);
		long double computeDirectionY(const Direction &direction);
		// Function to compute length of the snake
		long double computeSnakeLength(const std::deque<iPoint2D>& segments);
	};
	// Hash function for Point to use in unordered_map
	struct iPointHash2D {
		size_t operator()(const iPoint2D& p) const;
	};
	// Node structure for A* priority queue
	struct Node {
		iPoint2D point;
		int gCost; // Cost from start to this point
		int hCost; // Heuristic cost to target
		int fCost() const; // Total cost

		// Compare Nodes for priority queue (min-heap)
		bool operator<(const Node& other) const;
	};
	struct GameBoard : anex::IEntity
	{
		enum UseKeys
		{
			WSAD,
			UpDownLeftRight
		};
		int x;
		int y;
		int width;
		int height;
		int cellSize;
		UseKeys useKeys;
		std::shared_ptr<Snake> snake;
		iPoint2D fruit;
		int score = 0;
		bool gameOver = false;
		bool isAI = false;
		GameBoard(anex::IGame &game,
				  const int &x,
				  const int &y,
				  const int &width,
				  const int &height,
				  const int &cellSize,
				  const UseKeys &useKeys,
				  const bool &isAI);
		void render() override;
		void setFruitToRandom();
		std::vector<std::vector<bool>> getGrid() const;
		std::vector<iPoint2D> aStar(const iPoint2D &start, const iPoint2D &target) const;
	};
	struct MainMenuScene : anex::IScene
	{
		int borderWidth;
		int padding;
		unsigned int upKeyId = 0;
		unsigned int downKeyId = 0;
		unsigned int enterKeyId = 0;
		std::shared_ptr<ButtonEntity> playerVsAIButton;
		std::shared_ptr<ButtonEntity> trainAIButton;
		std::shared_ptr<ButtonEntity> playerVsPlayerButton;
		std::shared_ptr<ButtonEntity> singleplayerButton;
		std::shared_ptr<ButtonEntity> exitButton;
		std::vector<std::shared_ptr<ButtonEntity>> buttonsList;
		MainMenuScene(anex::IGame &game);
		~MainMenuScene();
		void positionButtons();
		void onUpKey(const bool &pressed);
		void onDownKey(const bool &pressed);
		void onEnterKey(const bool &pressed);
		void onPlayerVsAIEnter();
		void onTrainAIEnter();
		void onPlayerVsPlayerEnter();
		void onSingleplayerEnter();
		void onExitEnter();
	};
	struct SnakeScene : anex::IScene
	{
		bool gameStarted = true;
		std::vector<std::shared_ptr<GameBoard>> gameBoards;
		SnakeScene(anex::IGame &game, const unsigned int& boardsCount, const bool &player1IsAI = false, const bool &player2IsAI = false);
	};
}
