
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <time.h>

#include "Ai.h"
#include "Game.h"
#include "Utils.h"
#include "Hash.h"
#include "Trainer.h"

void InitAi(AiConfig* ai, bool constant) {
	for (int i = 0; i < 26; i++)
	{
		if (constant) {
			ai->BlotFactors[i] = 1.0;
			ai->ConnectedBlocksFactor[i] = 1.0;
		}
		else {
			ai->BlotFactors[i] = RandomDouble(&g_Rand, 0, 1);
			ai->ConnectedBlocksFactor[i] = RandomDouble(&g_Rand, 0, 1);
		}
	}
	ai->Id = 0;
	ai->SearchDepth = 1;
}

void InitAiManual(AiConfig* ai) {
	for (int i = 0; i < 26; i++)
	{
		ai->BlotFactors[i] = i / 5.0;// 51.37% advantage at depth 1.
		ai->ConnectedBlocksFactor[i] = 1.0;
	}
}

void SetDiceCombinations() {
	int i = 0;
	for (int a = 1; a < 7; a++)
	{
		for (int b = a; b < 7; b++)
		{
			AllDices[i][0] = a;
			AllDices[i++][1] = b;
		}
	}
}

bool PlayersPassedEachOther(Game* g) {
	int minBlack = 25;
	int maxWhite = 0;
	for (int i = 0; i < 26; i++)
	{
		if (g->Position[i] & Black)
			minBlack = min(minBlack, i);

		if (g->Position[i] & White)
			maxWhite = max(maxWhite, i);
	}
	return minBlack > maxWhite;
}

void RollDice(Game* g) {
	g->Dice[1] = genDice(&g->rand);// LlrandShift(g) % 6 + 1;
	g->Dice[0] = genDice(&g->rand); // LlrandShift(g) % 6 + 1;
}

bool ToHome(Move move) {
	return move.color == Black && move.to == 25 || move.color == White && move.to == 0;
}

// Must be called before the move of checkers is performed.
void AddHash(Game* g, Move move, bool hit) {

	g->Hash ^= PositionHash[move.color >> 5][move.from][CheckerCount(g->Position[move.from])]; // Counting checkers BEFORE count updated gives the correct number for the moved checker.
	//printf("%llu\n", PositionHash[move.color >> 5][move.from][CheckerCount(g->Position[move.from])]);

	if (hit) {
		// First checker of the move color is added to the to-position
		g->Hash ^= PositionHash[move.color >> 5][move.to][1];
		//printf("%llu\n", PositionHash[move.color >> 5][move.to][1]);

		if (move.color == Black)
		{
			// Last white checker removed from to-position
			g->Hash ^= PositionHash[1][move.to][1];
			//printf("%llu\n", PositionHash[1][move.to][1]);

			// White checker added to its bar
			g->Hash ^= PositionHash[1][25][CheckerCount(g->Position[25]) + 1];
			//printf("%llu\n", PositionHash[1][25][CheckerCount(g->Position[25]) + 1]);

		}
		else {
			// Last black checker removed from to-position
			g->Hash ^= PositionHash[0][move.to][1];
			//printf("%llu\n", PositionHash[0][move.to][1]);

			// Black checker added to its bar.
			g->Hash ^= PositionHash[0][0][CheckerCount(g->Position[0]) + 1];
			//printf("%llu\n", PositionHash[0][0][CheckerCount(g->Position[0]) + 1]);

		}
	}
	else {
		// Counting checkers AFTER count updated gives the correct number for the moved checker.
		g->Hash ^= PositionHash[move.color >> 5][move.to][CheckerCount(g->Position[move.to]) + 1];
		//printf("%llu\n", PositionHash[move.color >> 5][move.to][CheckerCount(g->Position[move.to]) + 1]);
	}
}

bool DoMove(Move move, Game* g) {
	ASSERT_DBG(move.from >= 0 && move.from <= 25 && move.to >= 0 && move.to <= 25 && (move.color == Black || move.color == White));
	/*char tmp[100];
	WriteGameString(tmp, g);*/

	ushort to = move.to;
	ushort from = move.from;
	bool toHome = ToHome(move);
	PlayerSide other = OtherColor(move.color);
	int toCount = CheckerCount(g->Position[to]);
	bool hit = toCount == 1 && (g->Position[to] & other) && !toHome;
	AddHash(g, move, hit);

	if (hit)
		g->Position[to] = 0;

	if (toHome) {
		move.color == Black ? g->BlackHome++ : g->WhiteHome++;
		ASSERT_DBG(g->BlackHome <= 15 && g->WhiteHome <= 15);
	}
	else {
		g->Position[to]++;
		g->Position[to] |= move.color;
		ASSERT_DBG(CheckerCount(g->Position[to]) <= 15);
	}

	g->Position[from]--;
	int fromCount = CheckerCount(g->Position[from]);
	if (fromCount == 0)
		g->Position[from] = 0; // re-setting color aswell.

	if (move.color == Black)
	{
		g->BlackLeft -= (move.to - move.from);
		ASSERT_DBG(g->BlackLeft >= 0);
		if (hit)
		{
			g->WhiteLeft += (25 - move.to);
			ASSERT_DBG(g->WhiteLeft > 0);
			g->Position[25] ++;
			g->Position[25] |= White;

			ASSERT_DBG(CheckerCount(g->Position[25]) <= 15 && CheckerCount(g->Position[25]) >= 0);
		}
	}
	else if (move.color == White) {
		g->WhiteLeft -= (move.from - move.to);
		ASSERT_DBG(g->WhiteLeft >= 0);
		if (hit)
		{
			g->BlackLeft += move.to; // += 25 - (25 - move.to);
			ASSERT_DBG(g->BlackLeft >= 0);

			g->Position[0] ++;
			g->Position[0] |= Black;

			ASSERT_DBG(CheckerCount(g->Position[0]) <= 15 && CheckerCount(g->Position[0]) >= 0);
		}
	}

	ASSERT_DBG(!CheckerCountAssert || (CountAllCheckers(Black, g) == 15 && CountAllCheckers(White, g) == 15));
	return hit;
}

void UndoMove(Move move, bool hit, Game* g, U64 prevHash) {
	ASSERT_DBG(move.from >= 0 && move.from <= 25 && move.to >= 0 && move.to <= 25 && (move.color == Black || move.color == White));
	g->Hash = prevHash;
	//use for bug tracking
	/*char* tmp[100];
	WriteGameString(tmp, g);*/

	ushort to = move.to;
	ushort from = move.from;
	bool fromHome = ToHome(move);

	g->Position[from]++;
	g->Position[from] |= move.color;

	if (fromHome) {
		move.color == Black ? g->BlackHome-- : g->WhiteHome--;
		ASSERT_DBG(g->BlackHome >= 0 && g->WhiteHome >= 0);
	}
	else {
		g->Position[to]--;
		if (CheckerCount(g->Position[to]) == 0)
			g->Position[to] = 0;
		ASSERT_DBG(g->Position[to] >= 0);

		if (hit)
		{
			g->Position[to] = 1 | OtherColor(move.color);
		}
	}

	if (move.color == Black)
	{
		g->BlackLeft += (move.to - move.from);
		ASSERT_DBG(g->BlackLeft >= 0);

		if (hit)
		{
			g->WhiteLeft -= (25 - move.to);
			ASSERT_DBG(g->WhiteLeft >= 0);
			g->Position[25]--;
			if (CheckerCount(g->Position[25]) == 0)
				g->Position[25] = 0;
			ASSERT_DBG(CheckerCount(g->Position[25]) >= 0);
		}
	}
	if (move.color == White)
	{
		g->WhiteLeft += (move.from - move.to);
		ASSERT_DBG(g->WhiteLeft >= 0);

		if (hit)
		{
			g->BlackLeft -= move.to; // -= 25 - (25 - move.to);
			ASSERT_DBG(g->BlackLeft >= 0);
			g->Position[0]--;
			if (CheckerCount(g->Position[0]) == 0)
				g->Position[0] = 0;
			ASSERT_DBG(CheckerCount(g->Position[0]) >= 0);
		}
	}
	ASSERT_DBG(!CheckerCountAssert || (CountAllCheckers(Black, g) == 15 && CountAllCheckers(White, g) == 15));
}

bool IsBlockedFor(ushort pos, ushort color, Game* g) {
	if (pos >= 25 || pos <= 0)
		return false;

	return (g->Position[pos] & OtherColor(color)) && (CheckerCount(g->Position[pos]) >= 2);
}

bool IsBlackBearingOff(ushort* lastCheckerPos, Game* g) {
	for (ushort i = 0; i <= 24; i++)
	{
		if ((g->Position[i] & Black) && CheckerCount(g->Position[i]) > 0)
		{
			*lastCheckerPos = i;
			return i >= 19;
		}
	}
	return true;
}

bool IsWhiteBearingOff(ushort* lastCheckerPos, Game* g) {
	for (ushort i = 25; i >= 1; i--)
	{
		if ((g->Position[i] & White) && CheckerCount(g->Position[i]) > 0)
		{
			*lastCheckerPos = i;
			return i <= 6;
		}
	}
	return true;
}

bool HashSetExists(U64 hash, Game* g) {
	// todo, try performance to loop in reverse.
	// but dont compare last added hash
	for (int i = 0; i < g->MoveSetsCount - 1; i++)
	{
		if (g->PossibleMoveSets[i].Hash == hash)
			return true;
	}
	return false;
}

void SetLightScore(Game* g, MoveSet* moveSet) {
	int score = g->BlackLeft - g->WhiteLeft;
	char a = g->CurrentPlayer >> 5;

	for (int i = 0; i < moveSet->Length; i++)
	{
		int to = moveSet->Moves[i].to;
		if (CheckerCount(g->Position[to]) == 1)
			score -= (int)AI(a).BlotFactors[to];
	}
	moveSet->score = score;
}

void CreateBlackMoveSets(int fromPos, int diceIdx, int diceCount, int* maxSetLength, Game* g) {
	int start = fromPos;
	int nextStart = fromPos;
	bool checkerFound = false;
	ushort lastCheckerPos;
	bool bearingOff = IsBlackBearingOff(&lastCheckerPos, g);
	if (bearingOff)
		start = max(start, 19);

	int toIndex = CheckerCount(g->Position[0]) > 0 ? 0 : 25;
	for (int i = start; i <= toIndex; i++)
	{
		if (!checkerFound)
			nextStart = i;

		if (!(g->Position[i] & Black))
			continue;

		checkerFound = true;
		int diceVal = diceIdx > 1 ? g->Dice[0] : g->Dice[diceIdx];
		int toPos = i + diceVal;

		// N�r man b�r av, f�r man anv�nda t�rningar med f�r h�gt v�rde,
		// men bara p� den checker l�ngst fr�n home.
		if (IsBlockedFor(toPos, Black, g))
			continue;

		if (bearingOff) {
			if (toPos > 25 && i != lastCheckerPos)
				continue;

			if (toPos > 25 && i == lastCheckerPos)
				toPos = 25;
		}
		else { //Not bearing off
			if (toPos > 24)
				continue;
		}

		// Atleast one move set is created.
		if (g->MoveSetsCount == 0)
			g->MoveSetsCount = 1;
		ushort setIdx = g->MoveSetsCount - 1;
		MoveSet* moveSet = &g->PossibleMoveSets[setIdx];
		Move* move = &moveSet->Moves[diceIdx];

		if (moveSet->Duplicate)
		{
			//Duplicates are reset. Not moving to next set here.
			moveSet->Length = diceIdx;
			moveSet->Duplicate = false;
		}
		else if (move->color != 0) {
			// A move is already generated for this dice in this sequence. Branch off a new set of moves.
			if (g->MoveSetsCount >= MAX_SETS_LENGTH)
			{
				printf("\nWarning. Max number of move sets reached.");
				return;
			}
			// But first set a light score for ordering			
			SetLightScore(g, moveSet);
			int copyCount = diceIdx;
			moveSet = &g->PossibleMoveSets[setIdx + 1];
			moveSet->Length = copyCount;

			if (copyCount > 0)
				memcpy(&moveSet->Moves[0], &g->PossibleMoveSets[setIdx].Moves[0], copyCount * sizeof(Move));

			move = &moveSet->Moves[diceIdx];
			g->MoveSetsCount++;
			ASSERT_DBG(g->MoveSetsCount < MAX_SETS_LENGTH);
		}

		// Creating move
		move->from = i;
		move->to = toPos;
		move->color = Black;
		moveSet->Length++;

		//This is returned to caller. So short sets can be removed later.
		//Special backgammon rule.
		*maxSetLength = max(*maxSetLength, moveSet->Length);

		if (diceIdx < diceCount - 1) {
			Move m = *move;
			U64 prevHash = g->Hash;
			int hit = DoMove(m, g);
			// Recursivle go on to next dice. Been looking for simpler ways.
			CreateBlackMoveSets(nextStart, diceIdx + 1, diceCount, maxSetLength, g);
			UndoMove(m, hit, g, prevHash);
		}
		else {
			// Last dice here
			U64 prevHash = g->Hash;
			bool hit = CheckerCount(g->Position[move->to]) == 1 && (g->Position[move->to] & OtherColor(move->color));
			// It should be faster to just calculate the hash rather than 
			AddHash(g, *move, hit);
			moveSet->Hash = g->Hash;
			g->Hash = prevHash;
			if (HashSetExists(moveSet->Hash, g))
			{
				moveSet->Duplicate = true;
			}
		}
	}
}

void CreateWhiteMoveSets(int fromPos, int diceIdx, int diceCount, int* maxSetLength, Game* g) {
	int start = fromPos;
	int nextStart = fromPos;
	bool checkerFound = false;
	ushort lastCheckerPos;
	bool bearingOff = IsWhiteBearingOff(&lastCheckerPos, g);
	if (bearingOff)
		start = min(start, 6);

	int toIndex = CheckerCount(g->Position[25]) > 0 ? 25 : 0;

	for (int i = start; i >= toIndex; i--)
	{
		if (!checkerFound)
			nextStart = i;

		if (!(g->Position[i] & White))
			continue;

		checkerFound = true;
		int diceVal = diceIdx > 1 ? g->Dice[0] : g->Dice[diceIdx];
		int toPos = i - diceVal;

		// N�r man b�r av, f�r man anv�nda t�rningar med f�r h�g summa
		// Men bara p� den checker l�ngst fr�n home.
		if (IsBlockedFor(toPos, White, g))
			continue;

		if (bearingOff) {
			if (toPos < 0 && i != lastCheckerPos)
				continue;

			if (toPos < 0 && i == lastCheckerPos)
				toPos = 0;
		}
		else { //Not bearing off
			if (toPos < 1)
				continue;
		}

		// Atleast one move set is created.
		if (g->MoveSetsCount == 0)
			g->MoveSetsCount = 1;
		ushort setIdx = g->MoveSetsCount - 1;
		MoveSet* moveSet = &g->PossibleMoveSets[setIdx];
		Move* move = &moveSet->Moves[diceIdx];

		if (moveSet->Duplicate)
		{
			moveSet->Length = diceIdx;
			moveSet->Duplicate = false;
		}
		else if (move->color != 0) {
			// A move is already generated for this dice in this sequence. Branch off a new sequence.
			if (g->MoveSetsCount >= MAX_SETS_LENGTH)
			{
				printf("\nWarning. Max number of move sets reached.");
				return;
			}
			SetLightScore(g, moveSet);
			int copyCount = diceIdx;
			moveSet = &g->PossibleMoveSets[setIdx + 1];
			moveSet->Length = copyCount;

			if (copyCount > 0)
				memcpy(&moveSet->Moves[0], &g->PossibleMoveSets[setIdx].Moves[0], copyCount * sizeof(Move));
			move = &moveSet->Moves[diceIdx];
			g->MoveSetsCount++;
			ASSERT_DBG(g->MoveSetsCount < MAX_SETS_LENGTH);
		}

		move->from = i;
		move->to = toPos;
		move->color = White;

		moveSet->Length++;
		*maxSetLength = max(*maxSetLength, moveSet->Length);

		if (diceIdx < diceCount - 1) {
			Move m = *move;
			U64 prevHash = g->Hash;
			int hit = DoMove(m, g);
			CreateWhiteMoveSets(nextStart, diceIdx + 1, diceCount, maxSetLength, g);
			UndoMove(m, hit, g, prevHash);
		}
		else {
			// Last dice here.
			U64 prevHash = g->Hash;
			bool hit = CheckerCount(g->Position[move->to]) == 1 && (g->Position[move->to] & OtherColor(move->color));
			AddHash(g, *move, hit); // It should be faster to just calculate the hash rather than to the move
			moveSet->Hash = g->Hash;
			g->Hash = prevHash;
			if (HashSetExists(moveSet->Hash, g))
			{
				moveSet->Duplicate = true;
			}
		}
	}

}

void ReverseDice(Game* g) {
	short temp = g->Dice[0];
	g->Dice[0] = g->Dice[1];
	g->Dice[1] = temp;
}

//Removes move sets that are shorter.
//They are not valid because a move that prohibits next move is not legal if there are other moves that can be made that are not prohibiting them.
void RemoveShorterSets(int maxSetLength, Game* g) {
	bool modified = false;
	int realCount = g->MoveSetsCount;
	do
	{
		modified = false;
		for (int i = 0; i < realCount; i++)
		{
			MoveSet* set = &g->PossibleMoveSets[i];
			if (set->Length < maxSetLength)
			{
				memcpy(&g->PossibleMoveSets[i], &g->PossibleMoveSets[i + 1], (realCount - i) * sizeof(MoveSet));
				modified = true;
				realCount--;
				break;
			}
		}
	} while (modified);
	g->MoveSetsCount = realCount;
}

void CreateMoves(Game* g) {

	// 50% Better performance than for loop
	memset(&g->PossibleMoveSets, 0, sizeof(g->PossibleMoveSets));

	g->MoveSetsCount = 0;
	// Largest Dice first
	ASSERT_DBG(g->Dice[0] >= 1 && g->Dice[0] <= 6 && g->Dice[1] >= 1 && g->Dice[1] <= 6);
	if (g->Dice[1] > g->Dice[0]) {
		ReverseDice(g);
	}

	int diceCount = g->Dice[0] == g->Dice[1] ? Settings.DiceQuads : 2;

	int maxSetLength = 0;
	for (size_t i = 0; i < 2; i++)
	{
		// TODO: Maybe reset sets here.
		maxSetLength = 0;
		if (g->CurrentPlayer & Black)
			CreateBlackMoveSets(0, 0, diceCount, &maxSetLength, g);
		else
			CreateWhiteMoveSets(25, 0, diceCount, &maxSetLength, g);

		//If no moves are found and dicecount == 2 reverse dice order and try again.
		if (g->MoveSetsCount == 0 && diceCount == 2) {
			ReverseDice(g);
		}
		else {
			break;
		}
	}

	RemoveShorterSets(maxSetLength, g);
}

int EvaluateCheckers(Game* g, PlayerSide color) {
	int score = 0;
	int blockCount = 0;
	bool playersPassed = PlayersPassedEachOther(g);

	char ai = color >> 5;
	// TODO: Try calculate both colors in same loop. Better performance?
	for (int i = 1; i < 25; i++)
	{
		int p = color == White ? 25 - i : i;
		short v = g->Position[p];
		int checkCount = CheckerCount(v);
		if (checkCount > 1 && (v & color)) {
			blockCount++;
		}
		else {
			if (blockCount && !playersPassed) {
				score += (int)pow((double)blockCount, AI(ai).ConnectedBlocksFactor[p]);
			}
			blockCount = 0;
		}

		if (checkCount == 1 && (v & color) && !playersPassed)
		{
			score -= (int)AI(ai).BlotFactors[p];
		}
	}
	return score;
}

int GetScore(Game* g) {
	int bHome = 10000 * g->BlackHome;
	int wHome = 10000 * g->WhiteHome;
	// positive for white, neg for black.
	g->EvalCounts++;
	return wHome - bHome + EvaluateCheckers(g, White) - EvaluateCheckers(g, Black) - g->WhiteLeft + g->BlackLeft;
}

//Gets the averege best score for the other player
int GetProbablilityScore(Game* g, int depth, int best_black, int best_white) {
	int totalScore = 0;
	g->CurrentPlayer = OtherColor(g->CurrentPlayer);
	short diceBuf[2] = { g->Dice[0], g->Dice[1] };
	for (int i = 0; i < DiceCombos; i++)
	{
		g->Dice[0] = AllDices[i][0];
		g->Dice[1] = AllDices[i][1];

		int score = RecursiveScore(g, depth, best_black, best_white);
		int m = g->Dice[0] == g->Dice[1] ? 1 : 2;
		totalScore += score * m;
	}
	// Since we are faking the dice down the stack, it is safer to but them back here.
	g->Dice[0] = diceBuf[0]; g->Dice[1] = diceBuf[1];
	g->CurrentPlayer = OtherColor(g->CurrentPlayer);
	return totalScore / DiceCombos;
}

// Moves the next best moveset first, and skips sets that are evaluated
void PickNextMoveSet(int moveNum, MoveSet* moveSets, int moveCount) {
	PlayerSide color = moveSets->Moves[0].color;
	int bestScore = color == White ? -INFINITY : INFINITY;
	int bestNum = moveNum;

	for (int index = moveNum; index < moveCount; ++index) {
		int score = moveSets[index].score;
		if ((color == White && score > bestScore) || (color == Black && score < bestScore)) {
			bestScore = score;
			bestNum = index;
		}
	}

	MoveSet temp = moveSets[moveNum];
	moveSets[moveNum] = moveSets[bestNum];
	moveSets[bestNum] = temp;
}

int RecursiveScore(Game* g, int depth, int best_black, int best_white) {
	int bestIdx = 0;
	int bestScore = g->CurrentPlayer == White ? -INFINITY : INFINITY;

	CreateMoves(g);

	if (g->MoveSetsCount == 0)
	{
		// This is not good for the current player.		
		return GetScore(g);
	}

	int setsCount = g->MoveSetsCount;
	MoveSet* localSets = malloc(sizeof(MoveSet) * g->MoveSetsCount);
	memcpy(localSets, &g->PossibleMoveSets, sizeof(MoveSet) * setsCount);
	for (int i = 0; i < setsCount; i++)
	{
		//only minor performance improvement, maybe enable if greater depths.
		//PickNextMoveSet(i, localSets, setsCount);
		MoveSet set = localSets[i];
		if (set.Duplicate)
			continue;

		Move moves[4];
		bool hits[4];
		U64 prevHash = g->Hash;
		for (int m = 0; m < set.Length; m++)
		{
			moves[m] = set.Moves[m];
			hits[m] = DoMove(moves[m], g);
		}

		int score;
		if (depth <= 0)
			score = GetScore(g);
		else
			// The best score the opponent can get. Minimize it.
			score = GetProbablilityScore(g, depth - 1, best_black, best_white);

		if (g->CurrentPlayer == White) {
			// White is maximizing
			if (score > bestScore)
			{
				bestScore = score;
				bestIdx = i;
				if (score > best_white) {
					if (score >= best_black) {
						for (int u = set.Length - 1; u >= 0; u--)
							UndoMove(moves[u], hits[u], g, prevHash);
						free(localSets);
						return best_black;
					}
					best_white = score;
				}
			}
		}
		else {
			// Black is minimizing
			if (score < bestScore) {
				bestScore = score;
				bestIdx = i;
				if (score < best_black) {
					if (score <= best_white) {
						for (int u = set.Length - 1; u >= 0; u--)
							UndoMove(moves[u], hits[u], g, prevHash);
						free(localSets);
						return best_white;
					}
					best_black = score;
				}
			}
		}

		//Undoing in reverse
		for (int u = set.Length - 1; u >= 0; u--)
			UndoMove(moves[u], hits[u], g, prevHash);
	}

	free(localSets);
	return bestScore;
}

int FindBestMoveSet(Game* g, MoveSet* bestSet, int depth) {
	int bestIdx = 0;
	int bestScore = g->CurrentPlayer == White ? -INFINITY : INFINITY;
	CreateMoves(g);

	if (g->MoveSetsCount == 0)
	{
		// This is not good for the current player.		
		return -1;
	}

	int setsCount = g->MoveSetsCount;
	MoveSet* localSets = malloc(sizeof(MoveSet) * g->MoveSetsCount);
	PlayerSide color = g->CurrentPlayer;
	memcpy(localSets, &g->PossibleMoveSets, sizeof(MoveSet) * setsCount);
	for (int i = 0; i < setsCount; i++)
	{
		MoveSet set = localSets[i];
		if (set.Duplicate)
			continue;

		Move moves[4];
		bool hits[4];
		U64 prevHash = g->Hash;
		for (int m = 0; m < set.Length; m++)
		{
			moves[m] = set.Moves[m];
			hits[m] = DoMove(moves[m], g);
		}

		int score;
		if (depth == 0)
			score = GetScore(g);
		else
			score = GetProbablilityScore(g, depth - 1, INFINITY, -INFINITY);

		if (color == White) {
			// White is maximizing
			if (score > bestScore)
			{
				bestScore = score;
				bestIdx = i;
			}
		}
		else {
			// Black is minimizing
			if (score < bestScore) {
				bestScore = score;
				bestIdx = i;
			}
		}

		//Undoing in reverse
		for (int u = set.Length - 1; u >= 0; u--)
			UndoMove(moves[u], hits[u], g, prevHash);
	}

	bestSet->Length = localSets[bestIdx].Length;
	bestSet->Duplicate = localSets[bestIdx].Duplicate;
	bestSet->Hash = localSets[bestIdx].Hash;
	for (int i = 0; i < 4; i++)
	{
		bestSet->Moves[i].from = localSets[bestIdx].Moves[i].from;
		bestSet->Moves[i].to = localSets[bestIdx].Moves[i].to;
		bestSet->Moves[i].color = localSets[bestIdx].Moves[i].color;
	}
	free(localSets);
	return bestIdx;
}

void Pause(Game* g) {
	char buf[BUF_SIZE];
	SetCursorPosition(0, 0);
	PrintGame(g);
	fgets(buf, 5000, stdin);
}


void PrintSet(MoveSet set) {
	printf("\nmove ");
	for (int i = 0; i < set.Length; i++)
	{
		Move m = set.Moves[i];
		if (m.from == 0 || m.from == 25)
			printf("bar");
		else
			printf("%d", m.from);
		printf("-");
		if (m.to == 0 || m.to == 25)
			printf("off");
		else
			printf("%d", m.to);

		if (i < set.Length - 1)
			printf(" ");
		else
			printf("\n");
	}
	fflush(stdout);
}

// Playes one game, optimally waits for user input and prints each game state.
void PlayGame(Game* g) {

	StartPosition(g);
	RollDice(g);
	//First roll of a game can not be equal.
	while (g->Dice[0] == g->Dice[1])
		RollDice(g);

	//Highest dice diceds who starts.
	g->CurrentPlayer = g->Dice[0] > g->Dice[1] ? Black : White;
	while (g->BlackLeft > 0 && g->WhiteLeft > 0 && g->Turns < Settings.MaxTurns)
	{
		/*if (Settings.PausePlay)
			Pause(g);*/
		g->EvalCounts = 0;
		int depth = Settings.SearchDepth;
		char buf[5000];
		MoveSet bestSet;
		if (FindBestMoveSet(g, &bestSet, depth) >= 0)
		{
			if (Settings.PausePlay)
			{
				PrintSet(bestSet);				
				fgets(buf, 5000, stdin);
			}
			for (int i = 0; i < bestSet.Length; i++)
			{
				DoMove(bestSet.Moves[i], g);
				ASSERT_DBG(CountAllCheckers(Black, g) == 15 && CountAllCheckers(White, g) == 15);
				if (Settings.PausePlay)
					Pause(g);
			}
		}
		g->CurrentPlayer = OtherColor(g->CurrentPlayer);
		g->Turns++;
		RollDice(g);
	}
}

// Runs the best trained AI agains an untrained.
void PlayAndEvaluate() {
	AiConfig untrained;
	InitAi(&untrained, true);
	Settings.DiceQuads = 4;
	Settings.MaxTurns = 400;
	Settings.PausePlay = false;
	Settings.SearchDepth = 1;

	InitTrainer();

	InitAiManual(&Trainer.Set[0]);

	CompareAIs(Trainer.Set[0], untrained);	
}

void WatchGame()
{
	InitAiManual(&AIs[0]);
	InitAiManual(&AIs[1]);
	Settings.PausePlay = true;
	Settings.SearchDepth = 2;
	PlayGame(&G);
	Settings.PausePlay = false;
}