﻿using Backend.Dto;
using Backend.Dto.admin;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Threading.Tasks;

namespace Backend.Controllers
{
    [ApiController]
    public class AdminController : AuthorizedController
    {
        [Route("api/admin/allgames")]
        [HttpGet]
        public PlayedGameListDto AllGames(string afterDate)
        {
            AssertAdmin();
            DateTime date;
            if (!DateTime.TryParse(afterDate, out date))
                date = DateTime.Parse("2300-01-01");

            // A trick to prevent loading same game twice.
            // Will only fail if two games are started the same millisecond which seems impossible.
            date = date.AddMilliseconds(-1);

            using (var db = new Db.BgDbContext())
            {
                var playedGames = db.Games.Select(g => new
                {
                    started = g.Started,
                    black = g.Players.Where(p => p.Color == Db.Color.Black),
                    white = g.Players.Where(p => p.Color == Db.Color.White),
                    winner = g.Winner
                }).Select(pl => new PlayedGameDto
                {
                    started = pl.started,
                    black = pl.black.First().User.Name,
                    white = pl.white.First().User.Name,
                    winner = (PlayerColor)pl.winner
                })
                .Where(x => x.started < date)
                .OrderByDescending(x => x.started)
                .Take(30);

                var games = playedGames.ToArray();
                return new PlayedGameListDto
                {
                    games = games
                };
            }
        }

        [Route("api/admin/summary")]
        [HttpGet]
        public SummaryDto Summary()
        {
            AssertAdmin();
            var summary = new SummaryDto();
            summary.ongoingGames = GamesService.AllGames.Count;
            using (var db = new Db.BgDbContext())
            {
                var today = DateTime.Now.Date;
                summary.playedGamesToday = db.Games.Count(g => g.Started > today);
                summary.playedGamesTotal = db.Games.Count();
                summary.reggedUsers = db.Users.Count();
                summary.reggedUsersToday = db.Users.Count(u => u.Registered > today);
            }

            return summary;
        }
    }
}
