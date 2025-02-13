﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Backend.Dto
{
    public enum ActionNames
    {
        gameCreated,
        dicesRolled,
        movesMade,
        gameEnded,
        opponentMove,
        undoMove,
        connectionInfo,
        gameRestore,
        resign,
        exitGame,
        requestedDoubling,
        acceptedDoubling,
        rolled
    }
}
