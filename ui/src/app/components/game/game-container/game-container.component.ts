import {
  AfterViewInit,
  ChangeDetectorRef,
  Component,
  ElementRef,
  HostListener,
  OnDestroy,
  ViewChild
} from '@angular/core';
import { Router } from '@angular/router';
import { Observable, of, Subscription } from 'rxjs';
import {
  DiceDto,
  GameDto,
  GameState,
  MoveDto,
  PlayerColor,
  UserDto
} from 'src/app/dto';
import { AccountService, SocketsService } from 'src/app/services';
import { AppState } from 'src/app/state/app-state';
import { StatusMessage } from 'src/app/dto/local/status-message';
import { Busy } from 'src/app/state/busy';
import { StatusMessageService } from 'src/app/services/status-message.service';
import { Sound } from 'src/app/utils';
import { map } from 'rxjs/operators';

@Component({
  selector: 'app-game',
  templateUrl: './game-container.component.html',
  styleUrls: ['./game-container.component.scss']
})
export class GameContainerComponent implements OnDestroy, AfterViewInit {
  constructor(
    private service: SocketsService,
    private accountService: AccountService,
    private router: Router,
    private statusMessageService: StatusMessageService,
    private changeDetector: ChangeDetectorRef
  ) {
    this.gameDto$ = AppState.Singleton.game.observe();
    this.dices$ = AppState.Singleton.dices.observe();
    this.diceSubs = AppState.Singleton.dices
      .observe()
      .subscribe(this.diceChanged.bind(this));
    this.playerColor$ = AppState.Singleton.myColor.observe();
    this.gameSubs = AppState.Singleton.game
      .observe()
      .subscribe(this.gameChanged.bind(this));
    this.rolledSubs = AppState.Singleton.rolled
      .observe()
      .subscribe(this.opponentRolled.bind(this));

    this.oponnetDoneSubs = AppState.Singleton.opponentDone
      .observe()
      .subscribe(this.oponnentDone.bind(this));
    this.message$ = AppState.Singleton.statusMessage.observe();
    this.timeLeft$ = AppState.Singleton.moveTimer.observe();
    this.user$ = AppState.Singleton.user.observe();

    // if game page is refreshed, restore user from login cookie
    if (!AppState.Singleton.user.getValue()) {
      this.accountService.repair();
    }

    const gameId = this.router.parseUrl(this.router.url).queryParams['gameId'];
    const playAi = this.router.parseUrl(this.router.url).queryParams['playAi'];
    const forGold = this.router.parseUrl(this.router.url).queryParams[
      'forGold'
    ];
    service.connect(gameId, playAi, forGold);
    this.playAiFlag = playAi === 'true';
    this.forGodlFlag = forGold === 'true';
    this.lokalStake = 0;
    // For some reason i could not use an observable. Maybe ill figure out why someday.
    this.themeName = AppState.Singleton.user.getValue()?.theme ?? 'dark';
  }

  gameDto$: Observable<GameDto>;
  dices$: Observable<DiceDto[]>;
  playerColor$: Observable<PlayerColor>;
  message$: Observable<StatusMessage>;
  timeLeft$: Observable<number>;
  user$: Observable<UserDto>;
  themeName: string;

  gameSubs: Subscription;
  diceSubs: Subscription;
  rolledSubs: Subscription;
  oponnetDoneSubs: Subscription;

  started = false;
  width = 450;
  height = 450;
  rollButtonClicked = false;
  diceColor: PlayerColor | null = PlayerColor.neither;
  messageCenter = 0;
  flipped = false;
  playAiFlag = false;
  forGodlFlag = false;
  PlayerColor = PlayerColor;
  lokalStake = 0;
  animatingStake = false;
  playAiQuestion = false;
  dicesDto: DiceDto[] | undefined;
  nextDoublingFactor = 1;

  @ViewChild('dices') dices: ElementRef | undefined;
  @ViewChild('boardButtons') boardButtons: ElementRef | undefined;
  @ViewChild('messages') messages: ElementRef | undefined;

  sendMoves(): void {
    this.service.sendMoves();
    this.rollButtonClicked = false;
    this.dicesVisible = false;
  }

  doMove(move: MoveDto): void {
    this.service.doMove(move);
    this.service.sendMove(move);
  }

  undoMove(): void {
    this.service.undoMove();
    this.service.sendUndo();
  }

  myTurn(): boolean {
    return AppState.Singleton.myTurn();
  }

  doublingRequested(): boolean {
    return AppState.Singleton.doublingRequested();
  }

  oponnentDone(): void {
    this.dicesVisible = false;
  }

  startedHandle: any;
  gameChanged(dto: GameDto): void {
    if (!this.started && dto) {
      clearTimeout(this.startedHandle);
      this.started = true;
      this.playAiQuestion = false;
      if (dto.isGoldGame) Sound.playCoin();
    }
    // console.log(dto?.id);
    this.setRollButtonVisible();
    this.setSendVisible();
    this.setUndoVisible();
    this.setDoublingVisible(dto);
    this.diceColor = dto?.currentPlayer;
    this.fireResize();
    this.newVisible = dto?.playState === GameState.ended;
    this.exitVisible =
      dto?.playState !== GameState.playing &&
      dto?.playState !== GameState.requestedDoubling;
    this.nextDoublingFactor = dto?.goldMultiplier * 2;

    this.animateStake(dto);
  }

  animateStake(dto: GameDto) {
    if (dto && dto.isGoldGame && dto.stake !== this.lokalStake) {
      this.animatingStake = true;
      const step = Math.ceil((dto.stake - this.lokalStake) / 10);
      setTimeout(() => {
        const handle = setInterval(() => {
          this.lokalStake += step;
          this.changeDetector.detectChanges();

          if (
            (step > 0 && this.lokalStake >= dto.stake) ||
            (step < 0 && this.lokalStake <= dto.stake)
          ) {
            clearInterval(handle);
            this.lokalStake = dto.stake;
            this.animatingStake = false;
          }
        }, 100);
      }, 100); // Give time to show everything
    }
  }

  setDoublingVisible(gameDto: GameDto) {
    if (!gameDto) return;
    this.acceptDoublingVisible =
      gameDto.isGoldGame &&
      gameDto.playState === GameState.requestedDoubling &&
      this.myTurn();
    // Visible if it is a gold-game and if it is my turn to double.
    const turn = AppState.Singleton.myColor.getValue() !== gameDto.lastDoubler;
    const rightType = gameDto.isGoldGame;
    this.requestDoublingVisible =
      turn &&
      rightType &&
      this.myTurn() &&
      this.rollButtonVisible &&
      gameDto.isGoldGame &&
      this.hasFundsForDoubling(gameDto);
  }

  hasFundsForDoubling(gameDto: GameDto): boolean {
    return (
      gameDto.blackPlayer.gold >= gameDto.stake / 2 &&
      gameDto.whitePlayer.gold >= gameDto.stake / 2
    );
  }

  diceChanged(dto: DiceDto[]): void {
    this.dicesDto = dto;
    this.setRollButtonVisible();
    this.setSendVisible();
    this.setUndoVisible();
    this.fireResize();
    const game = AppState.Singleton.game.getValue();
    this.exitVisible =
      game?.playState !== GameState.playing &&
      game?.playState !== GameState.requestedDoubling;
  }

  ngOnDestroy(): void {
    this.gameSubs.unsubscribe();
    this.diceSubs.unsubscribe();
    this.rolledSubs.unsubscribe();
    this.oponnetDoneSubs.unsubscribe();
    clearTimeout(this.startedHandle);
    AppState.Singleton.game.clearValue();
    AppState.Singleton.myColor.clearValue();
    AppState.Singleton.dices.clearValue();
    AppState.Singleton.messages.clearValue();
    AppState.Singleton.moveTimer.clearValue();
    this.started = false;
    this.service.exitGame();
  }

  moveAnimFinished(): void {
    this.service.shiftMoveAnimationsQueue();
  }

  @HostListener('window:resize', ['$event'])
  onResize(): void {
    this.width = Math.min(window.innerWidth, 1024);
    const span = this.messages?.nativeElement as Element;
    const spanWidth = span.getElementsByTagName('span')[0].clientWidth;
    this.messageCenter = this.width / 2 - spanWidth / 2;

    this.height = Math.min(window.innerHeight - 40, this.width * 0.6);

    const buttons = this.boardButtons?.nativeElement as HTMLElement;
    const btnsOffset = 5; //Cheating. Could not get the height.
    if (buttons) {
      buttons.style.top = `${this.height / 2 - btnsOffset}px`;
      buttons.style.right = `${this.width * 0.11}px`;
    }

    const dices = this.dices?.nativeElement as HTMLElement;
    if (dices) {
      // Puts the dices on right side if its my turn.
      if (this.myTurn()) {
        dices.style.left = `${this.width / 2 + 20}px`;
        dices.style.right = '';
      } else {
        dices.style.right = `${this.width / 2 + 20}px`;
        dices.style.left = '';
      }
      dices.style.top = `${this.height / 2 - btnsOffset}px`;
    }
  }

  ngAfterViewInit(): void {
    this.playAiQuestion = false;
    this.lokalStake = 0;

    if (!this.playAiFlag) this.waitForOpponent();
    this.fireResize();
  }

  private waitForOpponent() {
    Sound.playBlues();
    this.startedHandle = setTimeout(() => {
      if (!this.started) {
        this.playAiQuestion = true;
      }
    }, 11000);
  }

  fireResize(): void {
    setTimeout(() => {
      this.onResize();
    }, 1);
  }

  rollButtonVisible = false;
  sendVisible = false;
  undoVisible = false;
  dicesVisible = false;
  newVisible = false;
  exitVisible = true;
  acceptDoublingVisible = false;
  requestDoublingVisible = false;

  rollButtonClick(): void {
    this.service.sendRolled();
    this.rollButtonClicked = true;
    this.setRollButtonVisible();
    this.dicesVisible = true;

    Sound.playDice();

    this.setSendVisible();
    this.fireResize();
    this.requestDoublingVisible = false;
    const gme = AppState.Singleton.game.getValue();
    if (!gme.validMoves || gme.validMoves.length === 0) {
      this.statusMessageService.setBlockedMessage();
    }
    this.changeDetector.detectChanges();
  }

  opponentRolled(): void {
    this.dicesVisible = true;
    Sound.playDice();
  }

  setRollButtonVisible(): void {
    if (!this.myTurn() || this.doublingRequested()) {
      this.rollButtonVisible = false;
      return;
    }

    this.rollButtonVisible = !this.rollButtonClicked;
  }

  setSendVisible(): void {
    if (!this.myTurn() || !this.rollButtonClicked || this.doublingRequested()) {
      this.sendVisible = false;
      return;
    }

    const game = AppState.Singleton.game.getValue();
    this.sendVisible = !game || game.validMoves.length == 0;
  }

  setUndoVisible(): void {
    if (!this.myTurn() || this.doublingRequested()) {
      this.undoVisible = false;
      return;
    }

    const dices = AppState.Singleton.dices.getValue();
    this.undoVisible = dices && dices.filter((d) => d.used).length > 0;
  }

  resignGame(): void {
    this.service.resignGame();
  }

  newGame(): void {
    this.newVisible = false;
    this.started = false;
    this.service.resetGame();
    this.service.connect('', this.playAiFlag, this.forGodlFlag);
    this.waitForOpponent();
  }

  exitGame(): void {
    clearTimeout(this.startedHandle);
    this.service.exitGame();
    Busy.hide();
    this.router.navigateByUrl('/lobby');
  }

  requestDoubling(): void {
    this.requestDoublingVisible = false;
    this.service.requestDoubling();
  }

  acceptDoubling(): void {
    this.acceptDoublingVisible = false;
    this.service.acceptDoubling();
  }

  getDoubling(color: PlayerColor): Observable<number> {
    return this.gameDto$.pipe(
      map((game) => {
        return game?.lastDoubler === color ? game?.goldMultiplier : 0;
      })
    );
  }

  playAi(): void {
    this.playAiQuestion = false;
    this.service.exitGame();
    this.service.connect('', true, this.forGodlFlag);
  }

  keepWaiting(): void {
    Sound.playBlues();
    this.playAiQuestion = false;
  }

  @HostListener('window:beforeunload', ['$event'])
  unloadHandler(event: Event) {
    return !this.started;
  }
}
