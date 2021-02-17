import { SimpleChanges } from '@angular/core';
import { EventEmitter, OnChanges, Output, ViewChild } from '@angular/core';
import { AfterViewInit, Component, ElementRef, Input } from '@angular/core';
import { MoveDto, GameDto, PlayerColor, GameState } from 'src/app/dto';
import { Rectangle, Point } from 'src/app/utils';
import { CheckerDrag } from './checker-drag';

@Component({
  selector: 'app-game-board',
  templateUrl: './game-board.component.html',
  styleUrls: ['./game-board.component.scss']
})
export class GameBoardComponent implements AfterViewInit, OnChanges {
  @ViewChild('canvas') public canvas: ElementRef | undefined;

  @Input() public width = 600;
  @Input() public height = 400;
  @Input() game: GameDto | null = null;
  @Input() myColor: PlayerColor | null = PlayerColor.black;
  @Output() addMove = new EventEmitter<MoveDto>();

  borderWidth = 8;
  barWidth = this.borderWidth * 2;
  sideBoardWidth = this.width * 0.1;
  rectBase = 0;
  rectHeight = 0;
  rectangles: Rectangle[] = [];
  blackHome: Rectangle = new Rectangle(0, 0, 0, 0, 25);
  whiteHome: Rectangle = new Rectangle(0, 0, 0, 0, 0);

  cx: CanvasRenderingContext2D | null = null;
  drawDirty = false;
  dragging: CheckerDrag | null = null;
  cursor: Point = new Point(0, 0);
  framerate = 25;

  constructor() {
    for (let r = 0; r < 26; r++) {
      this.rectangles.push(new Rectangle(0, 0, 0, 0, 0));
    }
  }

  ngAfterViewInit(): void {
    if (!this.canvas) {
      return;
    }

    setInterval(() => {
      if (this.drawDirty) {
        this.draw(this.cx);
        this.drawDirty = false;
      }
    }, 1000 / this.framerate);

    const canvasEl: HTMLCanvasElement = this.canvas.nativeElement;
    this.cx = canvasEl.getContext('2d');
    this.drawDirty = true;
  }

  ngOnChanges(changes: SimpleChanges): void {
    if (!this.canvas) {
      return;
    }

    this.drawDirty = true;
  }

  draw(cx: CanvasRenderingContext2D | null): void {
    if (!this.canvas) {
      return;
    }
    const canvasEl: HTMLCanvasElement = this.canvas.nativeElement;
    canvasEl.width = this.width;
    canvasEl.height = this.height;

    this.drawBoard(cx);
    this.drawCheckers(cx);
    this.drawMessage(cx);
    // this.drawRects(cx);
  }

  drawRects(cx: CanvasRenderingContext2D | null): void {
    if (!cx) {
      return;
    }
    cx.lineWidth = 1;
    cx.fillStyle = '#000';
    cx.strokeStyle = '#F00';

    for (let r = 0; r < this.rectangles.length; r++) {
      this.rectangles[r].draw(cx);
    }

    this.blackHome.draw(cx);
    this.whiteHome.draw(cx);
  }

  drawCheckers(cx: CanvasRenderingContext2D | null): void {
    if (!cx) {
      return;
    }

    if (!this.game) {
      return;
    }
    // console.log(this.game.points);

    const r = this.rectangles[0].width / 2;
    const chWidth = r * 0.67;

    for (let p = 0; p < this.game.points.length; p++) {
      const point = this.game.points[p];
      let checkerCount = point.checkers.length;
      const rect = this.rectangles.filter((r) => r.pointIdx === p)[0];

      //drawing the dragged checker
      const drawDrag = this.dragging && this.dragging.rect == rect;
      if (drawDrag) {
        checkerCount--;
        if (point.checkers[0].color === PlayerColor.black) {
          cx.fillStyle = '#000';
        } else {
          cx.fillStyle = '#fff';
        }
        cx.strokeStyle = '#28DD2E';
        cx.beginPath();
        cx.ellipse(this.cursor.x - chWidth / 2, this.cursor.y - chWidth / 2, chWidth, chWidth, 0, 0, 360);
        cx.closePath();
        cx.fill();
        cx.stroke();
      }

      if (rect.canBeMovedTo) {
        cx.beginPath();
        const y = p < 13 ? rect.y : rect.y + rect.height;
        cx.moveTo(rect.x, y);
        cx.lineTo(rect.x + rect.width, y);
        cx.closePath();
        cx.strokeStyle = '#28DD2E';
        cx.lineWidth = 2;
        cx.stroke();
      }

      const dist = Math.min(2 * chWidth, rect.height / checkerCount);

      cx.lineWidth = 2;

      for (let i = 0; i < checkerCount; i++) {
        const checker = point.checkers[i];
        // skipping checkers att home.
        if (
          (p === 0 && checker.color === PlayerColor.white) ||
          (p === 25 && checker.color === PlayerColor.black)
        ) {
          continue;
        }

        let x = rect.x + r;
        if (p === 0 || p === 25) {
          x = rect.x + chWidth / 2;
        }
        let y = 0;
        if (p < 13) {
          y = rect.y + chWidth + dist * i;
        } else {
          y = rect.y + rect.height - chWidth - dist * i;
        }
        if (checker.color === PlayerColor.black) {
          cx.fillStyle = '#000';
          cx.strokeStyle = '#FFF';
        } else {
          cx.fillStyle = '#FFF';
          cx.strokeStyle = '#000';
        }
        cx.beginPath();
        cx.ellipse(x, y, chWidth, chWidth, 0, 0, 360);
        cx.closePath();
        cx.fill();
        cx.stroke();
        if (rect.hasValidMove && i == checkerCount - 1 && !drawDrag) {
          cx.strokeStyle = '#28DD2E';
          cx.lineWidth = 1.5;
          cx.stroke();
        }
      }
    }

    // draw checkers reached home.
    const blackCount = this.game.points[25].checkers.filter(
      (c) => c.color === PlayerColor.black
    ).length;
    const whiteCount = this.game.points[0].checkers.filter(
      (c) => c.color === PlayerColor.white
    ).length;

    let x = this.width - this.sideBoardWidth + 4;
    let y = this.height - this.borderWidth - 4;
    cx.fillStyle = '#000';
    for (let i = 0; i < blackCount; i++) {
      cx.fillRect(x, y - i * 6, this.sideBoardWidth / 2, 5);
    }

    x = this.width - this.sideBoardWidth + 4;
    y = this.borderWidth + 4;
    cx.fillStyle = '#fff';
    for (let i = 0; i < whiteCount; i++) {
      cx.fillRect(x, y + i * 6, this.sideBoardWidth / 2, 5);
    }

    //draw homes if can be moved to
    if (this.blackHome.canBeMovedTo) {
      this.blackHome.drawBottom(cx);
    }

    if (this.whiteHome.canBeMovedTo) {
      this.whiteHome.drawTop(cx);
    }
  }

  //Draws the background and also set shape of all rectangles used for interaction.
  drawBoard(cx: CanvasRenderingContext2D | null): void {
    if (!cx) {
      return;
    }

    // color and line width
    cx.lineWidth = 1;
    cx.fillStyle = '#ccc';
    cx.fillRect(0, 0, this.width, this.height);

    cx.strokeStyle = '#000';
    this.rectBase =
      (this.width -
        this.barWidth -
        2 * this.borderWidth -
        this.sideBoardWidth * 2) /
      12;

    this.rectHeight = this.height * 0.42;
    const colors = ['#555', '#eee'];
    let colorIdx = 0;
    let x = this.borderWidth + this.sideBoardWidth;
    let y = this.borderWidth;

    //blacks bar
    this.rectangles[24].set(
      this.width / 2 - this.borderWidth,
      this.rectHeight / 2,
      this.borderWidth * 2,
      this.rectHeight / 2 + this.borderWidth,
      0
    );

    //whites bar
    this.rectangles[25].set(
      this.width / 2 - this.borderWidth,
      this.height / 2 + this.height * 0.08 - this.borderWidth,
      this.borderWidth * 2,
      this.rectHeight / 2,
      25
    );

    //blacks home
    this.blackHome.set(
      this.width - this.sideBoardWidth + 4,
      this.height - this.height * 0.44 - this.borderWidth - 4,
      this.sideBoardWidth / 2,
      this.height * 0.44,
      25
    );

    //white home
    this.whiteHome.set(
      this.width - this.sideBoardWidth + 4,
      this.borderWidth + 4,
      this.sideBoardWidth / 2,
      this.height * 0.44,
      0
    );

    //Top triangles
    for (let i = 0; i < 12; i++) {
      if (i === 6) {
        x += this.barWidth;
      }
      this.rectangles[i].set(x, y, this.rectBase, this.rectHeight, 12 - i);
      // cx.strokeRect(x, y, this.rectBase, this.rectHeight);

      cx.fillStyle = colors[colorIdx];
      cx.beginPath();
      cx.moveTo(x, y);
      x += this.rectBase / 2;
      cx.lineTo(x, this.rectHeight);
      x += this.rectBase / 2;
      cx.lineTo(x, y);
      cx.closePath();
      cx.fill();
      colorIdx = colorIdx === 0 ? 1 : 0;
    }

    //bottom
    colorIdx = colorIdx === 0 ? 1 : 0;
    y = this.height - this.borderWidth;
    x = this.borderWidth + this.sideBoardWidth;
    for (let i = 0; i < 12; i++) {
      if (i == 6) {
        x += this.barWidth;
      }

      this.rectangles[i + 12].set(
        x,
        y - this.rectHeight,
        this.rectBase,
        this.rectHeight,
        i + 13
      );
      cx.fillStyle = colors[colorIdx];
      cx.beginPath();
      cx.moveTo(x, y);
      x += this.rectBase / 2;
      cx.lineTo(x, y - this.rectHeight);
      x += this.rectBase / 2;
      cx.lineTo(x, y);
      cx.closePath();
      cx.fill();
      colorIdx = colorIdx === 0 ? 1 : 0;
    }

    // the border
    cx.lineWidth = this.borderWidth;
    cx.strokeStyle = '#888';
    cx.strokeRect(
      this.sideBoardWidth + this.borderWidth / 2,
      this.borderWidth / 2,
      this.width - 2 * this.sideBoardWidth - this.borderWidth,
      this.height - this.borderWidth
    );

    //the bar
    cx.fillStyle = '#888';
    cx.fillRect(
      this.width / 2 - this.barWidth / 2,
      0,
      this.barWidth,
      this.height
    );
  }

  drawMessage(cx: CanvasRenderingContext2D | null): void {
    if (!cx) {
      return;
    }
    let text = '';
    cx.fillStyle = '#000';
    cx.font = '14px Arial';
    if (!this.game) {
      text = 'Waiting for opponent to connect';
    } else if (this.game.playState === GameState.ended) {
      // console.log(this.myColor, this.game.winner);
      text =
        this.myColor === this.game.winner
          ? 'Congrats! You won.'
          : 'Sorry. You lost the game.';
    } else if (this.myColor === this.game.currentPlayer) {
      text = `Your turn to move.  (${PlayerColor[this.game.currentPlayer]})`;
    } else {
      text = `Waiting for ${PlayerColor[this.game.currentPlayer]} to move.`;
    }

    cx.fillText(
      text,
      this.sideBoardWidth + 2 * this.borderWidth + 8,
      this.height / 2
    );
  }

  onMouseDown(event: MouseEvent): void {
    // console.log('down', event);

    if (!this.game) {
      return;
    }
    if (this.game.playState === GameState.ended) {
      return;
    }
    if (this.myColor != this.game.currentPlayer) {
      return;
    }
    const { clientX, clientY } = event;
    for (let i = 0; i < this.rectangles.length; i++) {
      const rect = this.rectangles[i];
      const x = clientX - this.borderWidth;
      const y = clientY - this.borderWidth;
      if (!rect.contains(x, y)) {
        continue;
      }
      let ptIdx = rect.pointIdx;
      if (this.game?.currentPlayer === PlayerColor.white) {
        ptIdx = 25 - rect.pointIdx;
      }
      // The moves are ordered  by backend by dice value.
      const move = this.game.validMoves.find((m) => m.from === ptIdx);
      if (move !== undefined) {
        this.dragging = new CheckerDrag(rect, clientX, clientY, ptIdx);
        break;
      }
    }
  }

  onMouseMove(event: MouseEvent): void {
    // console.log('move', event);

    this.cursor.x = event.clientX;
    this.cursor.y = event.clientY;
    if (!this.game) {
      return;
    }
    if (this.myColor != this.game.currentPlayer) {
      return;
    }
    if (this.dragging) {
      this.drawDirty = true;
      return;
    }

    // Indicating what can be moved and where.
    const { clientX, clientY } = event;
    const isWhite = this.game.currentPlayer === PlayerColor.white;

    // resetting all
    this.rectangles.forEach((rect) => {
      rect.canBeMovedTo = false;
      rect.hasValidMove = false;
      this.whiteHome.canBeMovedTo = false;
      this.blackHome.canBeMovedTo = false;
    });

    for (let i = 0; i < this.rectangles.length; i++) {
      const rect = this.rectangles[i];
      const x = clientX - this.borderWidth;
      const y = clientY - this.borderWidth;
      if (!rect.contains(x, y)) {
        continue;
      }
      const ptIdx = isWhite ? 25 - rect.pointIdx : rect.pointIdx;
      const moves = this.game.validMoves.filter((m) => m.from === ptIdx);
      if (moves.length > 0) {
        rect.hasValidMove = true;
        moves.forEach((move) => {
          const toIdx = isWhite ? 25 - move.to : move.to;
          const pointRect = this.rectangles.find((r) => r.pointIdx === toIdx);
          // not marking bar when checker is going home
          if (pointRect && move.to < 25) {
            pointRect.canBeMovedTo = true;
          }

          if (move.to === 25) {
            if (move.color === PlayerColor.white) {
              this.whiteHome.canBeMovedTo = true;
            } else {
              this.blackHome.canBeMovedTo = true;
            }
          }
        });
      }
    }
    this.drawDirty = true;
  }

  onMouseUp(event: MouseEvent): void {
    // console.log('up', event);

    if (!this.game) {
      return;
    }
    if (this.game.playState === GameState.ended) {
      return;
    }
    if (this.myColor != this.game.currentPlayer) {
      return;
    }
    if (!this.dragging) {
      return;
    }
    const { clientX, clientY } = event;
    const { xDown, yDown, fromIdx } = this.dragging;

    this.dragging = null;
    // Unless the cursor has moved to far, this is a click event, and should move the move of the largest dice.
    const isClick =
      Math.abs(clientX - xDown) < 3 && Math.abs(clientY - yDown) < 3;

    const allRects: Rectangle[] = [...this.rectangles];
    if (this.game.currentPlayer === PlayerColor.black) {
      allRects.push(this.blackHome);
    } else {
      allRects.push(this.whiteHome);
    }

    for (let i = 0; i < allRects.length; i++) {
      const rect = allRects[i];
      const x = clientX - this.borderWidth;
      const y = clientY - this.borderWidth;
      if (!rect.contains(x, y)) {
        continue;
      }
      let ptIdx = rect.pointIdx;
      if (this.game?.currentPlayer === PlayerColor.white) {
        ptIdx = 25 - rect.pointIdx;
      }
      let move: MoveDto | undefined = undefined;
      if (isClick) {
        move = this.game.validMoves.find((m) => m.from === ptIdx);
      } else {
        move = this.game.validMoves.find(
          (m) => m.to === ptIdx && fromIdx === m.from
        );
      }

      if (move) {
        this.addMove.emit(move);
        break;
      }
    }
    this.drawDirty = true;
  }

  onMouseLeave(event: MouseEvent): void {
    // console.log('leave', event);
  }

  onClick(event: MouseEvent): void {
    // console.log('click', event);
  }
}
