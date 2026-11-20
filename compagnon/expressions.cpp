#include "expressions.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static const int PIN_SDA = 21;         // ligne de données I2C
static const int PIN_SCL = 22;         // ligne d'horloge I2C
static const uint8_t ADRESSE = 0x3C;   // adresse confirmée par le scan I2C

static Adafruit_SSD1306 ecran(128, 64, &Wire, -1);  // -1 : pas de broche de reset

bool expressionsInit() {
  Wire.begin(PIN_SDA, PIN_SCL);
  return ecran.begin(SSD1306_SWITCHCAPVCC, ADRESSE);
}

static const int CENTRE_Y = ZONE_VISAGE_Y + ZONE_VISAGE_HAUTEUR / 2;

// Un dessin par Expression : chaque ticket ne remplit que le sien.
// Tous dessinent dans la Zone visage (y >= 16) ; maintenantMs sert aux animations.
// Les Expressions Claude montrent la Mascotte Claude ; les autres montrent la Boule.

// ---- Boule : une petite boule blanche toute ronde, antenne à pompon, traits noirs sur le corps ----

static const int BOULE_X = 64;                     // centre horizontal
static const int YEUX_Y = 41;                      // hauteur des yeux (sans rebond)
static const int YEUX_ECART = 8;                   // demi-écart entre les deux yeux
static const unsigned long REBOND_PERIODE_MS = 250;

// Décalage vertical du rebond : 0 ou -2 px, alterne toutes les REBOND_PERIODE_MS
static int rebond(unsigned long tempsMs) { return (tempsMs / REBOND_PERIODE_MS) % 2 ? -2 : 0; }

// Corps (y 24..63, ou 22..61 en l'air) et antenne ; le pompon ne monte jamais au-dessus de y = 16
static void bouleCorps(int dy) {
  ecran.fillRoundRect(40, 24 + dy, 48, 40, 19, SSD1306_WHITE);
  ecran.drawFastVLine(BOULE_X, 20 + dy, 4, SSD1306_WHITE);
  ecran.fillCircle(BOULE_X, 20 + dy, 2, SSD1306_WHITE);
}

// Yeux ronds noirs avec un reflet ; un trait pendant le clignement. dyYeux : regard vers le haut (< 0)
static void bouleYeux(int dy, bool fermes, int dyYeux = 0) {
  for (int s = -1; s <= 1; s += 2) {
    int x = BOULE_X + s * YEUX_ECART, y = YEUX_Y + dy + dyYeux;
    if (fermes) {
      ecran.fillRect(x - 4, y, 9, 2, SSD1306_BLACK);
    } else {
      ecran.fillCircle(x, y, 3, SSD1306_BLACK);
      ecran.drawPixel(x + 1, y - 1, SSD1306_WHITE);
    }
  }
}

// Yeux heureux fermés en « ^ » (épais)
static void bouleYeuxHeureux(int dy) {
  for (int s = -1; s <= 1; s += 2) {
    int x = BOULE_X + s * YEUX_ECART, y = YEUX_Y + dy;
    for (int d = 0; d < 2; d++) {
      ecran.drawLine(x - 4, y + 2 + d, x, y - 2 + d, SSD1306_BLACK);
      ecran.drawLine(x, y - 2 + d, x + 4, y + 2 + d, SSD1306_BLACK);
    }
  }
}

// Petit sourire en « U » : un cercle dont on efface la moitié haute
static void bouleSourire(int dy) {
  ecran.drawCircle(BOULE_X, 47 + dy, 3, SSD1306_BLACK);
  ecran.fillRect(BOULE_X - 4, 44 + dy, 9, 3, SSD1306_WHITE);
}

static void bouleBoucheTrait(int dy) { ecran.drawFastHLine(BOULE_X - 3, 49 + dy, 7, SSD1306_BLACK); }
static void bouleBoucheO(int dy) { ecran.fillCircle(BOULE_X, 50 + dy, 2, SSD1306_BLACK); }

// Repos : la Boule sourit et cligne des yeux (ticket #6)
static void dessinerRepos(const EtatCourant& e, unsigned long maintenantMs) {
  (void)e;
  bouleCorps(0);
  bouleYeux(0, etatYeuxFermes(maintenantMs));
  bouleSourire(0);
}

// Endormi : yeux fermés (deux traits, jamais de clignement), bouche en « o », des « z » montent lentement.
static void dessinerEndormi(const EtatCourant& e, unsigned long maintenantMs) {
  (void)e;
  bouleCorps(0);
  bouleYeux(0, true);
  bouleBoucheO(0);
  const unsigned long CYCLE_MS = 4000;  // un « z » met 4 s à monter ; trois « z » décalés
  const int BAS = ZONE_VISAGE_Y + ZONE_VISAGE_HAUTEUR - 14, HAUT = ZONE_VISAGE_Y;
  for (int i = 0; i < 3; i++) {
    unsigned long p = (maintenantMs + i * (CYCLE_MS / 3)) % CYCLE_MS;
    ecran.setCursor(BOULE_X + 22 + (int)(p / 400) * 2, BAS - (int)((BAS - HAUT) * p / CYCLE_MS));
    ecran.print('z');
  }
}

// Mascotte Claude (toutes les Expressions Claude) : sprite pixel-art de 16 x 10 cases de M px.
// Corps 12 x 8 cases, bras de 2 cases de chaque côté (rangées 4-5), quatre pattes de 2 cases.
static const int M = 4;
static const int MASCOTTE_X = 64 - 8 * M;               // centrée horizontalement
static const int MASCOTTE_Y = ZONE_VISAGE_Y + 4;        // 40 px de haut : de 20 à 59
static const int OEIL_G = 4, OEIL_D = 11, OEIL_RANGEE = 2;  // colonnes et rangée des yeux

// Une case (ou un bloc de cases) de la Mascotte posée en (x, y)
static void caseMascotte(int x, int y, int col, int rangee, int largeur, int hauteur, uint16_t couleur) {
  ecran.fillRect(x + col * M, y + rangee * M, largeur * M, hauteur * M, couleur);
}

// Silhouette pleine, sans yeux ; brasLeve : bras droit relevé de deux rangées
static void dessinerMascotte(int x, int y, bool brasLeve) {
  caseMascotte(x, y, 2, 0, 12, 8, SSD1306_WHITE);
  caseMascotte(x, y, 0, 4, 2, 2, SSD1306_WHITE);
  caseMascotte(x, y, 14, brasLeve ? 2 : 4, 2, 2, SSD1306_WHITE);
  for (int col : {2, 4, 11, 13}) caseMascotte(x, y, col, 8, 1, 2, SSD1306_WHITE);
}

// claude_done : la Mascotte sautille, yeux en "^" (heureux)
static void dessinerContent(const EtatCourant& e, unsigned long maintenantMs) {
  int y = MASCOTTE_Y - (((maintenantMs - e.debutMs) / 300) % 2 ? 3 : 0);  // reste dans la Zone visage
  dessinerMascotte(MASCOTTE_X, y, false);
  for (int col : {OEIL_G, OEIL_D}) {
    int cx = MASCOTTE_X + col * M + M / 2, bas = y + (OEIL_RANGEE + 2) * M - 2;
    for (int d = 0; d < 2; d++) {
      ecran.drawLine(cx - 4, bas - d, cx, bas - 4 - d, SSD1306_BLACK);
      ecran.drawLine(cx, bas - 4 - d, cx + 4, bas - d, SSD1306_BLACK);
    }
  }
}

// claude_waiting : la Mascotte, grands yeux, agite le bras droit ; « ! » à côté
static void dessinerInterpelle(const EtatCourant& e, unsigned long maintenantMs) {
  bool brasLeve = ((maintenantMs - e.debutMs) / 300) % 2;
  dessinerMascotte(MASCOTTE_X, MASCOTTE_Y, brasLeve);
  caseMascotte(MASCOTTE_X, MASCOTTE_Y, OEIL_G - 1, OEIL_RANGEE, 2, 2, SSD1306_BLACK);
  caseMascotte(MASCOTTE_X, MASCOTTE_Y, OEIL_D, OEIL_RANGEE, 2, 2, SSD1306_BLACK);
  ecran.fillRect(112, ZONE_VISAGE_Y + 6, 4, 18, SSD1306_WHITE);
  ecran.fillRect(112, ZONE_VISAGE_Y + 28, 4, 4, SSD1306_WHITE);
}

// Casque de gamer : serre-tête en demi-cercle (haut à y = 18, reste dans la Zone visage),
// oreillettes cerclées de noir sur les côtés du corps, micro noir vers la bouche
static void bouleCasque() {
  for (int r = 25; r <= 26; r++) ecran.drawCircleHelper(BOULE_X, 44, r, 0x1 | 0x2, SSD1306_WHITE);
  for (int x : {34, 86}) {
    ecran.fillRoundRect(x, 38, 9, 15, 3, SSD1306_WHITE);
    ecran.drawRoundRect(x, 38, 9, 15, 3, SSD1306_BLACK);
  }
  ecran.drawLine(40, 50, 54, 53, SSD1306_BLACK);
  ecran.fillCircle(55, 53, 1, SSD1306_BLACK);
}

// Jeu : la Boule concentrée, casque de gamer sur la tête, yeux mi-clos sous des sourcils
// froncés (même clignement que le Repos), bouche serrée, petite manette en bas à droite
static void dessinerJeu(const EtatCourant& e, unsigned long maintenantMs) {
  (void)e;
  bool fermes = etatYeuxFermes(maintenantMs);
  bouleCorps(0);
  bouleCasque();
  bouleYeux(0, fermes);
  if (!fermes) ecran.fillRect(BOULE_X - 12, YEUX_Y - 4, 25, 4, SSD1306_WHITE);  // paupières
  ecran.drawLine(BOULE_X - 12, YEUX_Y - 7, BOULE_X - 5, YEUX_Y - 5, SSD1306_BLACK);  // sourcils
  ecran.drawLine(BOULE_X + 12, YEUX_Y - 7, BOULE_X + 5, YEUX_Y - 5, SSD1306_BLACK);
  bouleBoucheTrait(0);
  // manette : corps arrondi, croix à gauche, deux boutons à droite
  ecran.fillRoundRect(101, CENTRE_Y + 6, 25, 13, 4, SSD1306_WHITE);
  ecran.fillRect(104, CENTRE_Y + 11, 7, 3, SSD1306_BLACK);
  ecran.fillRect(106, CENTRE_Y + 9, 3, 7, SSD1306_BLACK);
  ecran.fillCircle(117, CENTRE_Y + 14, 1, SSD1306_BLACK);
  ecran.fillCircle(121, CENTRE_Y + 11, 1, SSD1306_BLACK);
}

// Une ligne du Bandeau : fixe si elle tient, sinon défile en boucle (2 copies côte à côte)
static void ligneDefilante(const char* texte, int y, unsigned long tempsMs) {
  int largeur = etatLargeurTexte(texte);
  int x = etatPositionDefilement(largeur, tempsMs);
  ecran.setCursor(x, y);
  ecran.print(texte);
  if (largeur > LARGEUR_ECRAN_PX) {
    ecran.setCursor(x + largeur + ECART_DEFILEMENT_PX, y);
    ecran.print(texte);
  }
}

// Une croche : tête pleine, hampe, drapeau
static void dessinerNote(int x, int y) {
  ecran.fillCircle(x, y, 3, SSD1306_WHITE);
  ecran.drawFastVLine(x + 3, y - 12, 12, SSD1306_WHITE);
  ecran.drawLine(x + 3, y - 12, x + 8, y - 8, SSD1306_WHITE);
}

// music : la Boule, yeux heureux fermés, se dandine entre des notes qui rebondissent
// (titre et artiste : voir le Bandeau)
static void dessinerMusique(const EtatCourant& e, unsigned long maintenantMs) {
  unsigned long t = maintenantMs - e.debutMs;
  int dy = rebond(t);
  bouleCorps(dy);
  bouleYeuxHeureux(dy);
  bouleSourire(dy);
  int haut = (t / NOTE_REBOND_PERIODE_MS) % 2 ? 3 : 0;
  dessinerNote(14, CENTRE_Y + 4 - haut);
  dessinerNote(112, CENTRE_Y - 6 + haut);
}

// silence (Rappel sans Morceau courant) : la Boule bouche serrée, une note barrée à côté
static void dessinerSilence(const EtatCourant& e, unsigned long maintenantMs) {
  (void)e;
  bouleCorps(0);
  bouleYeux(0, etatYeuxFermes(maintenantMs));
  bouleBoucheTrait(0);
  dessinerNote(106, CENTRE_Y + 8);
  ecran.drawLine(97, CENTRE_Y + 12, 119, CENTRE_Y - 8, SSD1306_WHITE);
}

// Sans Claude (Rappel Claude sans Session connue) : contour vide de la Mascotte, sans yeux ;
// le Bandeau dit « pas de Claude »
static void dessinerSansClaude(const EtatCourant& e, unsigned long maintenantMs) {
  (void)e;
  (void)maintenantMs;
  const int x = MASCOTTE_X, y = MASCOTTE_Y;
  dessinerMascotte(x, y, false);
  // Évidement : chaque bloc rentré de 1 px en noir ; l'union des évidements ne laisse que le contour
  ecran.fillRect(x + 2 * M + 1, y + 1, 12 * M - 2, 8 * M - 2, SSD1306_BLACK);
  ecran.fillRect(x + 1, y + 4 * M + 1, 16 * M - 2, 2 * M - 2, SSD1306_BLACK);
  for (int col : {2, 4, 11, 13}) ecran.fillRect(x + col * M + 1, y + 8 * M - 1, M - 2, 2 * M, SSD1306_BLACK);
}

// Travaille (Rappel Claude) : la Mascotte, yeux plissés (moitié basse), pattes qui trottinent,
// trois points qui tournent à côté
static void dessinerTravaille(const EtatCourant& e, unsigned long maintenantMs) {
  unsigned long t = maintenantMs - e.debutMs;
  dessinerMascotte(MASCOTTE_X, MASCOTTE_Y, false);
  caseMascotte(MASCOTTE_X, MASCOTTE_Y, OEIL_G, OEIL_RANGEE + 1, 1, 1, SSD1306_BLACK);
  caseMascotte(MASCOTTE_X, MASCOTTE_Y, OEIL_D, OEIL_RANGEE + 1, 1, 1, SSD1306_BLACK);
  // Une patte sur deux levée à tour de rôle : on efface le bas de deux pattes
  bool pas = (t / 200) % 2;
  for (int col : {2, 4, 11, 13})
    if ((col == 2 || col == 11) == pas) caseMascotte(MASCOTTE_X, MASCOTTE_Y, col, 9, 1, 1, SSD1306_BLACK);
  // Trois points sur un cercle de rayon 6 ; un quart de tour par PERIODE_ANIM_MS * 8
  int quart = (t / (PERIODE_ANIM_MS * 8)) % 4;
  static const int DX[4] = {6, 0, -6, 0};
  static const int DY[4] = {0, 6, 0, -6};
  for (int i = 0; i < 3; i++) {
    int k = (quart + i) % 4;
    ecran.fillCircle(113 + DX[k], CENTRE_Y + DY[k], 1, SSD1306_WHITE);
  }
}

// git_push : la Boule sautille, regarde en l'air, bouche en « o », une flèche monte à côté
static void dessinerPush(const EtatCourant& e, unsigned long maintenantMs) {
  unsigned long t = maintenantMs - e.debutMs;
  int dy = rebond(t);
  bouleCorps(dy);
  bouleYeux(dy, false, -3);
  bouleBoucheO(dy);
  int haut = etatPushFlecheY(t);  // toujours dans la Zone visage
  ecran.fillTriangle(108, haut, 100, haut + 9, 116, haut + 9, SSD1306_WHITE);
  ecran.fillRect(105, haut + 9, 7, PUSH_FLECHE_HAUTEUR - 9, SSD1306_WHITE);
}

void expressionsAfficher(const EtatCourant& e, const char* texteBandeau, unsigned long maintenantMs) {
  ecran.clearDisplay();
  ecran.setTextColor(SSD1306_WHITE);
  ecran.setTextSize(1);
  ecran.cp437(true);         // les textes de l'État sont déjà en CP437 (voir etatVersCp437)
  ecran.setTextWrap(false); // un texte trop long est coupé au bord, jamais replié sous le Bandeau

  // Bandeau, pour toutes les Expressions : une ligne centrée verticalement, ou deux lignes
  // (y = 0 et 8, police 8 px : rien ne dépasse y = 16). Chaque ligne trop large défile.
  unsigned long t = maintenantMs - e.debutMs;
  bool deuxLignes = e.ligne2[0] != '\0';
  if (texteBandeau != nullptr) ligneDefilante(texteBandeau, deuxLignes ? 0 : (BANDEAU_HAUTEUR - 8) / 2, t);
  if (deuxLignes) ligneDefilante(e.ligne2, 8, t);

  switch (e.expression) {
    case EXPR_CONTENT:    dessinerContent(e, maintenantMs); break;
    case EXPR_INTERPELLE: dessinerInterpelle(e, maintenantMs); break;
    case EXPR_MUSIQUE:    dessinerMusique(e, maintenantMs); break;
    case EXPR_PUSH:       dessinerPush(e, maintenantMs); break;
    case EXPR_JEU:        dessinerJeu(e, maintenantMs); break;
    case EXPR_SILENCE:    dessinerSilence(e, maintenantMs); break;
    case EXPR_SANS_CLAUDE: dessinerSansClaude(e, maintenantMs); break;
    case EXPR_TRAVAILLE:  dessinerTravaille(e, maintenantMs); break;
    case EXPR_ENDORMI:    dessinerEndormi(e, maintenantMs); break;
    case EXPR_REPOS:
    default:            dessinerRepos(e, maintenantMs); break;
  }

  ecran.display();
}
