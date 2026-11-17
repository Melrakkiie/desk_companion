test:
	g++ -std=c++17 -Wall -Wextra -o /tmp/test_etat tests/test_etat.cpp compagnon/etat.cpp && /tmp/test_etat
	bash tests/test_hooks.sh
	bash tests/test_musique.sh
	bash tests/test_jeu.sh
	bash tests/test_push.sh
