Tiến lên - Go forward
========
##### Cumulative hours spent (so far): 42.5h
## About
This is a C implementation of the Vietnamese card game "Tiến lên", also known as "Thirteen", "Killer" and other regional names. It features online-play functionality for up to four players using TCP sockets, allowing play over a network or a locally hosted machine.
The game was written in C, as a personal challenge, and to improve my C capabilities. The user interface was built using the [Ncurses](https://invisible-island.net/ncurses/) library, while everything else (networking, game logic, supporting systems) were implemented by myself.

## Setup
`server.c` and `client.c` need to be compiled using a standard C compiler, such as gcc, cmake or any other.   
#### server.c
`$ gcc server.c cards.c -o server -pthread`

#### client.c
`$ gcc client.c cards.c -lncursesw -o client`  

Run `server` on a machine, and connect via `client`, by typing in the corresponding IP. Locally hosted games are also possible, simply leave the IP field blank, when setting up the server, aswell as when you're connecting via the client.

## Game rules
This is a slightly modified implementation of the game, using [the standard rules](https://en.wikipedia.org/wiki/Ti%E1%BA%BFn_l%C3%AAn) and some house rules we (my friends and I) are playing by.
### Cards:
A standard deck of 52 cards is used. 13 cards are given to up to 4 players, even if less than 4 players are connected. The ranking of the cards is as follows:  
`3 4 5 6 7 8 9 10 J Q K A 2`  
The suit ranking is as follows:  
`♠ ♣ ♦ ♥`  
meaning the lowest possible card is a `3 of Aces`, and the highest being a `2 of Hearts`.  
For the first hand, a random player is chosen. For subsequent hands, the loser of the previous hand begins. Players in turn discard single cards, or card combinations to a central pile. The objective of the game is to be the last player to hold any cards.  
  
Valid combinations are:
1. A single card *(High card)*
2. A pair of cards *(Pair)*
3. A triplet of cards *(Trips)*
4. A quartet of cards *(Quads)*
5. A sequence of 3 or more *(Straight)* - suit doesn't matter
6. A double-sequence of 3 or more *(Two-pair straight)* - suit doesn't matter

A sequence may not "overflow", meaning that while `K A 2` is a valid sequence, `A 2 3` isn't.
Each player, in turn, may either play or pass. To play, they must contribute a card or combination to the pile that matches the type currently being played, but beating it in rank or suit. The highest suited card inside a pair or sequence determines the suit needed to beat it. So to beat a sequence ending with a `King of Spades`, a sequence ending with a `King of Diamonds` at minimium must be played, or naturally a higher ranking sequence.
Playing continues until there is a card or combination no one can beat, its player then starting the next round. Once a player has passed, they may not contribute to the pile again and have to wait until the next round. 
If a player finishes their cards on their turn, the turn must be continued by the next player. If the next player cannot match their card, they may start a new round.

### House rules

#### Bombs
Usually, when playing a *High card* round, a `2 of Hearts` is unbeatable, as naturally it is the highest card. However, so-called `Bombs` beat them.
1. A `single 2` can be beaten with a *double-sequence* of 3 or more pairs, **or** with a quartet.
2. A `pair of 2's` can be beaten with a *double-sequence* of 4 or more pairs, **or** with a quartet.
3. A `triplet of 2's` can be beaten with a *double-sequence* of 5 or more pairs.

#### Instant win
Having a quartet of 2's on hand, automatically wins the game, regardless of being played or not.

### TODO:
- add game logic
  
  client:
  - [ ] add ui element to signify what type of hand is being played right now
  - [ ] add pass button to ui (keybind? visual button?) -> currently playing nothing is a pass (might need to rework or not)
  - [ ] display score counter somewhere (maybe on connected users list)
  - [ ] display server messages indicating which player played what
  - [ ] display server message indicating player passed

  server:
  - [X] add waiting room for connecting players
  - [X] server needs to deal cards to players
  - [ ] add general game logic (including bombs, instant win etc.)
  - [ ] add turn based card laying by clients (playing cards or passing)
  - [ ] add score tracking (winning a game adds one point to the respective player)

future:
  - add special message if player won with `3 of aces`
  - maybe add pot system? (visual only)

