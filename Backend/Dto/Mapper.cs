﻿using Backend.Dto.account;
using Backend.Rules;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Backend.Dto
{
    public static class Mapper
    {
        public static GameDto ToDto(this Game game)
        {
            var gameDto = new GameDto
            {
                id = game.Id.ToString(),
                blackPlayer = game.BlackPlayer.ToDto(),
                whitePlayer = game.WhitePlayer.ToDto(),
                currentPlayer = (PlayerColor)game.CurrentPlayer,
                playState = (GameState)game.PlayState,
                points = game.Points.Select(p => p.ToDto()).ToArray(),
                validMoves = game.ValidMoves.Select(m => m.ToDto()).ToArray(),
                thinkTime = Game.ClientCountDown - (DateTime.Now - game.ThinkStart).TotalSeconds,
                goldMultiplier = game.GoldMultiplier,
                isGoldGame = game.IsGoldGame,
                lastDoubler = game.LastDoubler.HasValue ? (PlayerColor)game.LastDoubler : null,
                stake = game.Stake
            };
            return gameDto;
        }

        public static PlayerDto ToDto(this Player player)
        {
            var playerDto = new PlayerDto
            {
                // Do not mapp id, it should never be sent to opponent.
                playerColor = (PlayerColor)player.PlayerColor,
                name = player.Name,
                pointsLeft = player.PointsLeft,
                elo = player.Elo,
                gold = player.Gold,
                photoUrl = player.Photo
            };
            return playerDto;
        }

        public static PointDto ToDto(this Point point)
        {
            var pointDto = new PointDto
            {
                blackNumber = point.BlackNumber,
                whiteNumber = point.WhiteNumber,
                checkers = point.Checkers.Select(c => c.ToDto()).ToArray()                
            };
            return pointDto;
        }

        public static CheckerDto ToDto(this Checker checker)
        {
            var checkerDto = new CheckerDto
            {
                color = (PlayerColor)checker.Color
            };
            return checkerDto;
        }

        public static DiceDto ToDto(this Dice dice)
        {
            var diceDto = new DiceDto
            {
                used = dice.Used,
                value = dice.Value
            };
            return diceDto;
        }

        public static MoveDto ToDto(this Move move)
        {
            var moveDto = new MoveDto
            {
                color = (PlayerColor)move.Color,
                from = move.From.GetNumber(move.Color),
                to = move.To.GetNumber(move.Color),
                // recursing up in move tree
                nextMoves = move.NextMoves.Select(move => move.ToDto()).ToArray()
            };
            return moveDto;
        }

        public static Move ToMove(this MoveDto dto, Game game)
        {
            var color = (Player.Color)dto.color;
            return new Move
            {
                Color = (Player.Color)dto.color,
                From = game.Points.Single(p => p.GetNumber(color) == dto.from),
                To = game.Points.Single(p => p.GetNumber(color) == dto.to),
            };      
        }

        public static UserDto ToDto(this Db.User dbUser)
        {
            if (dbUser == null)
                return null;

            int unixTimestamp = (int)dbUser.LastFreeGold.Subtract(new DateTime(1970, 1, 1)).TotalSeconds;

            return new UserDto
            {
                email = dbUser.Email,
                name = dbUser.Name,
                id = dbUser.Id.ToString(),
                photoUrl = dbUser.PhotoUrl,
                showPhoto = dbUser.ShowPhoto,
                socialProvider = dbUser.SocialProvider,
                isAdmin = dbUser.Admin,
                socialProviderId = dbUser.ProviderId,
                preferredLanguage = dbUser.PreferredLanguage ?? "en",
                emailNotification = dbUser.EmailNotifications,
                theme = dbUser.Theme,
                gold = dbUser.Gold,
                lastFreeGold = unixTimestamp,
                elo = dbUser.Elo,
                passHash = dbUser.PassHash,
                localLoginName = dbUser.LocalLogin
            };
        }
    }
}
