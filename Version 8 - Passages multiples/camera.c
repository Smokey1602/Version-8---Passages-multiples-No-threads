// g++ -Wall -o camera camera.c -lrt ` pkg-config --cflags --libs opencv `
/*sed -i ‘s/if (DEFINED CMAKE_TOOLCHAIN_FILE)/if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)/g’ makefiles/cmake/arm-linux.cmake*/


#include "opencv2/core/core.hpp" //bibliotheque générale d'opencv
#include "opencv2/highgui/highgui.hpp" //bibilotheque auxilliaire(traitement d'image)
#include "opencv2/imgproc/imgproc.hpp" //bibliotheque auxilliaire(affichage des images)
#include "opencv2/opencv.hpp" // Root des bibilotheques
#include <stdlib.h>
#include <mysql/mysql.h>
#include <stdio.h>

#include <iostream> //bibliotheque de gestion des entrées video
#include <string.h>
#include <sys/time.h> //bibliotheque interne a la raspberry (permet de recupere la date et l'heure de la raspberry
#include <sys/io.h>
#include <unistd.h>

using namespace std;
using namespace cv;

//Déclaration des fonctions 

void recup_fond(void);
void get_time(void);
void sauvegarde_Journaliere(void);
void sauvegarde_Continue(void);
void suppressbruit(Mat Pic);

//////////////////////////

Mat source;
Mat source2;
Mat source3;
Mat source_crop[10];
Mat hsvsource;
Mat masquesource;
Mat sourcecrop;

Mat flux;
Mat foreground[15];
Mat fond[15];
Mat detection[15];
Mat Comptage[15];
Mat sourcegray;
Mat canny_out[15];
Mat HSV;

int LastXTotal[40]={-1}, LastYTotal[40]={-1}, posXTotal[40],posYTotal[40]; //tableau de variables (1case /image) permettant de tracer le mouvement d'une abeille dans un canal en temps réel sur un ecran <=> position actuelle de l'abeille
int deplacementTotal[40]={0};
int bisXTotal[40]={0};
int flagTotal[40]={0};
int CumulEntree=0,CumulSortie=0;//variables comptant les entrée sorties des abeilles sur une journée complète.
int CumulEntreeJour=0,CumulSortieJour=0;//variables comptant les entrée sorties des abeilles sur une journée complète.
int Entree=0,Sortie=0; 
int IDRuche=1;

int ZoneEntree[11][4]={{0}}; // Déclaration d'un tableau pour les zones d'entrée regroupant le numero de la zone comme index et ses 4 parametres comme coordonées
int ZoneSortie[11][4]={{0}};// Déclaration d'un tableau pour les zones de sorties regroupant le numero de la zone comme index et ses 4 parametres comme coordonées

char delimiteur[11][2]={{0}};
char flagSens[11]={0};
char flagAccept[11]={0};

int Y[40]={450,460,470,480,490,500,510,520,530,540,550,560,570,580,590,600,610,620,630,640};
int X[3]={300,330,0};
int nombreporte = 0;

// ---------------- Declaration des fonds pour la détection des abeilles --------------------------------------
int history = 1000;
float varThreshold = 16.f;
BackgroundSubtractorMOG2 bg[25] = BackgroundSubtractorMOG2(history,varThreshold,false);

// ---------------- Fin de la déclaration des fonds pour la détection des abeilles --------------------------------------

// --------- variables pour recupere l'heure permettant la sauvegarde -----------

static int seconds_last = 99; //variable permettant de determiner le changement de seconde(chargé avec 99 aléatoirement pour entrer une premeire fois dans la boucle)
char Date[20],Jour[20],Minute[20],SauvegardeDate[20],OldJour[20]={0},oldSauvegardeDate[20]={0};//variables dont nous allons nous servir dans tout le programme et nous permettant de mettre l'heure et la date dans des variables lisibles
string oldMinute="\0", oldDay="\0";


RNG rng(12345);
Scalar color = Scalar( rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255) );



// VideoCapture capture("Passage_19_09_17_2.avi"); //initialisation du flux(on le met ici car toutes les fonctions profiterons du flux video sans redéclaration
VideoCapture capture(-1);
double largeur = capture.get(CV_CAP_PROP_FRAME_WIDTH);
double hauteur = capture.get(CV_CAP_PROP_FRAME_HEIGHT);
double fps = capture.get(CV_CAP_PROP_BRIGHTNESS);

void get_time()//fonction nous permettant de recuperer la date et l heure de la raspberry
{
/*
	Présentation: Ceci est une fonction générique et modifiée permettant d'acceder a la date et l'heure de la raspberry
	Explications:
	1- nous récupérons la date actuelle
	2- on test voir si nous sommes a une nouvelle date (ici 1seconde plus tard)
	3- on met a jour notre flag de detection de nouvelle data
	4- nous récuperons et formatons toutes les odnénes de dates comme nous en avons besoin
	Précisions : Cette fonction est GENERIQUE elle marche sur tout les raspberry par defaut aucun paquet n est nécessaire
*/
	timeval curTime;
	gettimeofday(&curTime, NULL);
	if (seconds_last == curTime.tv_sec)
	return;
	
	seconds_last = curTime.tv_sec;
	strftime(Date, 80, "%d-%m-%Y", localtime(&curTime.tv_sec)); // date au format jj-mm-aa
	strftime(Jour, 80, "%A", localtime(&curTime.tv_sec));// recupération du jour (lundi,mardi ... dimanche)
	strftime(Minute, 80, "%M", localtime(&curTime.tv_sec));//récupération des minutes pour le déclanchement de la sauvegarde
	strftime(SauvegardeDate,80,"%Y-%m-%d %H:%M:%S",localtime(&curTime.tv_sec)); //récupération de la date au format aaaa-mm-jj hh:mm:ss
	
	if(Minute != oldMinute) //on teste l'heure pour savoir s'il faut sauvegarder les données.
	{
		sauvegarde_Continue();
		oldMinute = Minute;
		printf("le %s :: Entree: %d \tSorties: %d \tDelta: %d \n",SauvegardeDate,Entree,Sortie,Entree-Sortie);
		Entree=0;
		Sortie=0;
 	}
	if(Date!=oldDay )
	{
		sauvegarde_Journaliere();
		//recup_fond();
		oldDay=Date;
		printf("le %s :: Entreejour: %d \tSortiesjour: %d \tDeltajour: %d\n",oldSauvegardeDate,CumulEntree,CumulSortie,CumulEntree-CumulSortie);
		CumulEntree=0;
		CumulSortie=0;
	
	}
	strftime(oldSauvegardeDate,80,"%Y-%m-%d %H:%M:%S",localtime(&curTime.tv_sec)); //récupération de la date au format aaaa-mm-jj hh:mm:ss
}
void sauvegarde_Journaliere(void)
{

	MYSQL *mysql;
	char *sQLComplete = NULL;
	char *ChaineConcatene=NULL;
	
	char sIDRuche[10] = {0};
	char sEntree[10] = {0};
	char sSortie[10] = {0};
	char sDeltaES[10] = {0};
	
	sprintf(sIDRuche,"%d",IDRuche);
	sprintf(sEntree,"%d",CumulEntree);
	sprintf(sSortie,"%d",CumulSortie);
	sprintf(sDeltaES,"%d",CumulEntree-CumulSortie);
	
	
	//initialisation BDD

	mysql = mysql_init(NULL);
	if(mysql == NULL)
	{
		printf("impossible d'initialiser la BDD\n");
	}
	
	sQLComplete = (char*)malloc(strlen("INSERT INTO CompteurJournalier(IDRuche,Date,EntreesJournalieres,SortiesJournalieres,DeltaJournalier) VALUES('%s','%s','%s','%s','%s')")+400);
		
	if(NULL == mysql_real_connect(mysql,"10.2.2.58","root","proot","Ruche_Compteur",0,NULL,0))
	{

		printf("Erreur de connection avec la BDD \n");
		free(ChaineConcatene);
		ChaineConcatene = NULL; 
		free(sQLComplete);
		sQLComplete = NULL;
	}
	else
	{
		sprintf(sQLComplete,"INSERT INTO CompteurJournalier(IDRuche,Date,EntreesJournalieres,SortiesJournalieres,DeltaJournalier) VALUES('%s','%s','%s','%s','%s')",sIDRuche,SauvegardeDate,sEntree,sSortie,sDeltaES);
		if(mysql_query(mysql,sQLComplete))
		{

			printf("echec de l'envoi\n");
		}
	}
	mysql_close(mysql);	
}

void sauvegarde_Continue(void)
{

	MYSQL *mysql;
	char *sQLComplete = NULL;
	char *ChaineConcatene=NULL;

	char sIDRuche[10] = {0};
	char sEntree[10] = {0};
	char sSortie[10] = {0};
	char sDeltaES[10] = {0};
	
	sprintf(sIDRuche,"%d",IDRuche);
	sprintf(sEntree,"%d",Entree);
	sprintf(sSortie,"%d",Sortie);
	sprintf(sDeltaES,"%d",Entree-Sortie);
	
	
	//initialisation BDD

	mysql = mysql_init(NULL);
	if(mysql == NULL)
	{
		printf("impossible d'initialiser la BDD\n");
	}
	
	sQLComplete = (char*)malloc(strlen("INSERT INTO CompteurContinu(IDRuche,Date,Entrees,Sortie,DeltaES) VALUES('%s','%s','%s','%s','%s')")+400);
		
	if(NULL == mysql_real_connect(mysql,"10.2.2.58","root","proot","Ruche_Compteur",0,NULL,0))
	{

		printf("Erreur de connection avec la BDD \n");
		free(ChaineConcatene);
		ChaineConcatene = NULL; 
		free(sQLComplete);
		sQLComplete = NULL;
	}
	else
	{
		sprintf(sQLComplete,"INSERT INTO CompteurContinu(IDRuche,Date,Entrees,Sorties,DeltaES) VALUES('%s','%s','%s','%s','%s')",sIDRuche,SauvegardeDate,sEntree,sSortie,sDeltaES);
		if(mysql_query(mysql,sQLComplete))
		{

			printf("echec de l'envoi\n");
		}
	}
	mysql_close(mysql);	
}

void suppressbruit(Mat Pic)
{
/*
	Présentation : Ceci est une fonction donnée par OpenCV. Elle permet de réduire le bruit des images que nous
	traitons. En quelques sortes en regarde dans un cercle quel pixel est le plus présent et fait de cercle un cercle 
	de la couleur qui est la plus présente.
	Explications : N/A
	Précisions : N/A

*/ 
	erode(Pic, Pic, getStructuringElement(MORPH_ELLIPSE, Size(2,2)) );
  	dilate(Pic, Pic, getStructuringElement(MORPH_ELLIPSE, Size(2,2)) ); 
	blur(Pic,Pic,Size(3,3));
	//fastNlMeansDenoising(Pic,Pic,3,7,21);
	
}

void calib_auto()
{
/*
	Présentation :
	Cette fonction ne prenant aucun de parametres et ne retournant rien nous permet d effectuer une calibration automatique
	des portes d'entrées sortie. 
	Explication : 
	1-Nous prennons l'image sortant du flux video(qui est normalement l'entrée de la ruche vue du dessus)
	2-Nous traitons cette image pour ne garder que la couleur "rouge"
	3-Nous prenons une ligne de l'image et nous sucrutons la totalitée de cette ligne
	4-Nous scrutons les données au fur et a mesure qu'elle arrivent et les rangeons dans un tableau.
	Précisions:
	etape 4: -> en toute logique, nous avons dans le tableau tout les moments où la ligne de pixel change de couleur
	cad que lorsque que l'on a decouvert le pixel 0 nosu enregistrons l'endroit ou nous sommes dans la ligne
	et ensuite nous ne fesons rien mais des que nous trouvons un pixel a 255 nous reenregistrons cette position qui
	marquera la fin de la detection de la porte "1".
*/


	int flag0=0,flag255=0,ecart=0,matj=0,nbporte=0,i=0,tmp=0;
	int calibauto[80]={0};
	int sourcecalib=0;
	
	int zone=0;

	
	for(sourcecalib=0;sourcecalib<70;sourcecalib++)
	{
		waitKey(1);
		capture >> source;
	}
	
	
	//imshow("videocalib",source);
	cvtColor(source,hsvsource,CV_BGR2HSV);
	inRange(hsvsource,Scalar(90,50,50,0),Scalar(130,255,255,0),masquesource);
	suppressbruit(masquesource);
	//line(masquesource, Point(330, 0), Point(330, 480), Scalar(255,255,255), 1);
	//imshow("masksourceB",masquesource);
	//printf("%d %d",masquesource.cols,masquesource.rows);
	
		for(matj=0;matj<masquesource.rows;matj++)
		{	
			switch(masquesource.at<uchar>(330,matj))
			{
				case 0:
				if(flag0==0 && ecart >10) // 0=> noir donc la couleur n'est pas présente
				{
					flag0=1;flag255=0;ecart=0;
					calibauto[nbporte]=matj;
					nbporte++;
				}break;
				
				case 255:
				if(flag255==0 && ecart >10) //255 => blanc donc la couleur est présente
				{
					flag0=0;flag255=1;ecart=0;
					calibauto[nbporte]=matj;
					nbporte++;
				}break;
				
			}
		ecart++;
			
		}
		/* on détectera a tout les coups les 4 changements de couleurs (changement de couleurs logiques et attendus)
			de ce fait le tableau sera rempli comme suit : 
			[0, début de la premiere ligne de couleur, fin de la premier ligne de couleur, debut de la seconde ligne de couleur, fin de la seconde ligne de couleur]
		*/		
			
	X[0]=calibauto[2]+20; // retiens la position du "premier" trait bleu 
	X[1]=calibauto[3]-20; // retiens la position du "deuxieme" trait bleu
	X[2]=(calibauto[3]+calibauto[2])/2;
	// printf("%d \n",calibauto[3]-calibauto[2]);
	
	ecart=0;flag0=0;flag255=0;//raz des valeurs
	nbporte=0,tmp=X[0]+25; // Déclaration d'une variable qui sera le milieu de la zone de détection 
	//printf("%d \n",tmp);
	
	cvtColor(source,hsvsource,CV_BGR2HSV);
	inRange(hsvsource,Scalar(50,35,35,0),Scalar(70,200,200,0),masquesource);
	suppressbruit(masquesource);
	//line(masquesource, Point(0, tmp), Point(640, tmp), Scalar(255,255,255), 2);
	//imshow("masksourceV",masquesource);
	
		for(matj=0;matj<masquesource.cols;matj++)
		{	
			switch(masquesource.at<uchar>(matj,tmp))
			{
				case 0:
				if(flag0==0 && ecart >10)
				{
					flag0=1;flag255=0;ecart=0;
					calibauto[nbporte]=matj;
					nbporte++;
				}break;
				
				case 255:
				if(flag255==0 && ecart >10)
				{
					
					flag0=0;flag255=1;ecart=0;
					calibauto[nbporte]=matj;
					nbporte++;
				}break;
				
			}
		ecart++;
			
		}
		
		nombreporte=(nbporte-3)/2;
		for(i=0;i<nombreporte*2;i++)
		{
				Y[i]=calibauto[i+2]-5; // le "-5" est la pour faire le décalage induit par le test précédent ainsi la position des portes est bien respecté
				
		}
	
		if(nombreporte<=0)
		{
			nombreporte=10;
		}	
		
	// On recupère maintenant toutes les coordonnées pour créer un tableau regroupant les espaces de passage des abeilles
	ZoneEntree[0][0]=X[0];
	ZoneEntree[0][1]=X[1];
	ZoneEntree[0][2]=Y[0];
	ZoneEntree[0][3]=Y[2];
	for(zone=0;zone<11;zone++)
	{
		ZoneEntree[zone][0]=Y[zone*2];
		ZoneEntree[zone][1]=Y[zone*2+1];
		ZoneEntree[zone][2]=X[0];
		ZoneEntree[zone][3]=X[2];		
	}
	
	for(zone=0;zone<11;zone++)
	{
		ZoneSortie[zone][0]=Y[zone*2];
		ZoneSortie[zone][1]=Y[zone*2+1];
		ZoneSortie[zone][2]=X[2];
		ZoneSortie[zone][3]=X[1];		
	}
	
	// for(zone=0;zone<11;zone++)
	// {
		// rectangle(source,Point(ZoneEntree[zone][2],ZoneEntree[zone][0]),Point(ZoneEntree[zone][3],ZoneEntree[zone][1]),Scalar(255,255,0,50),-1);
		// rectangle(source,Point(ZoneSortie[zone][2]+1,ZoneSortie[zone][0]),Point(ZoneSortie[zone][3],ZoneSortie[zone][1]),Scalar(255,0,0,30),-1);
	// }
	// printf("nbporte : %d \n",nombreporte);
	// imshow("Zones",source);
}

void calibration(Mat CSource,int affichage)
{
/*

	Présentation : Cette fonction nous permet de modifier les tailles de nos portes.
	Explications :
	1-Nous creeons deja des nouvelles feneters temporaire et utilisée uniquement dans cette fonction 
	2-Nous lancons un while pour gere dynamiquement les parametres de fenetres
	3-Nous creeons des barres de selections fesant bouger en meme temps les lignes 
	4-Nous avons en direct le rendu de la calibration
	5-Nous attendons la demande d'extinction pour quitter la fonction et nous detruisons les fenetres temporaire
	en meme temps
	Précisions : createTrackbar -> permet d'implémenter les curseurs de sélection
		     line -> permet de dessiner les lignes sur l'image principale

*/

	int trackbar=0;
	int coulLigne=0;
	Mat Calib;
	
	transpose(CSource,Calib);
	//imshow("tourne",Calib);
		for(trackbar=0;trackbar<nombreporte*2;trackbar++)
		{
			
			if(coulLigne>=2)
			{
				line(CSource, Point(0, Y[trackbar]), Point(640, Y[trackbar]), Scalar(0,255,255), 2);
				coulLigne=0;trackbar++;
				line(CSource, Point(0, Y[trackbar]), Point(640, Y[trackbar]), Scalar(0,255,255), 2);
			}
			else
			{
				line(CSource, Point(0, Y[trackbar]), Point(640, Y[trackbar]), Scalar(0,255,0), 2);
				coulLigne++;
			}
		
		}	

		line(CSource, Point(X[0],0 ), Point(X[0], 480), Scalar(0,0,0), 2);
		line(CSource, Point(X[1],0 ), Point(X[1], 480), Scalar(0,0,0), 2);
		
		if(affichage == 1)
		{
			imshow("Calibration",CSource); //Affichage ou non des portes
		}	

}
void ColorZone(int i,Mat image,int *posX,int *posY,int *LastX,int *LastY, int zone)//dessine les lignes pour suivi d objet
{
/*
	Présentation: on pourrait croire cette fonction inutile vu son nom.. mais en fait elle est le coeur de la detection
	En effet elle permet de determiner le centre de l'abeille lors de son passage dans la porte.
	Explications:
	1-On défini un moment et nous nous en servons pour determiner une position relative du point dans l'image
	2-On se sert de ces moments pour recuperer la coordonnée du point que nous enregistrons dans une variable
	3-On fait un test improbable de sécuritée pour eviter d'avoir des données n'existant pas(negatives)
	4-On met en tampon la position de l'abeille.
	Précisions: l'affichage de la ligne rouge n'est PAS obligatoire. elle est la pour présentation de la detection et
	ne consomme aucune ressource processeur (ou tellement infime qu'elle est négligeable...
*/
	Moments Moments = moments(image);

  	double M01 = Moments.m01;
 	double M10 = Moments.m10;
 	double Area = Moments.m00;

       // si area <= 400, cela signifie que l'objet est trop petit pour etre detecté 
	if (Area > 200)
 	{
	//calcule le centre du point
   	posX[i] = (M10 / Area)+X[0];
   	posY[i] = (M01 / Area)+Y[i*2];        
        
		if (posX[i]>ZoneEntree[i][2] && posX[i]<ZoneEntree[i][3] && zone==1 )
   		{
    		// dessine une ligne route entre le point a t=0 et a t=t+1
			rectangle(source, Point(ZoneEntree[i][2]+1,ZoneEntree[i][0]),Point(ZoneEntree[i][3]-15,ZoneEntree[i][1]), Scalar(255,0,255), -1);   
			delimiteur[i][0]=1;
   		}
	
		if (posX[i]>ZoneEntree[i][2] && posX[i]<ZoneEntree[i][3] && zone==2)
   		{
    		//dessine une ligne route entre le point a t=0 et a t=t+1
			rectangle(source, Point(ZoneSortie[i][2]+15,ZoneSortie[i][0]),Point(ZoneSortie[i][3],ZoneSortie[i][1]), Scalar(0,255,255), -1); 
			delimiteur[i][1]=1;
   		}
		
		
    	LastY[i] = posY[i];
		LastX[i] = posX[i];
  	}
	else
	{
		if(zone==1 )
		{
			delimiteur[i][0]=0;
		}
		if(zone==2 )
		{
			delimiteur[i][1]=0;
		}
	}
  	 //imshow("flux_video2", source); //show the original image
	
}
void passage(int i,int *deplacement,int *bisX, int *LastX,int *flag, int *entree, int *sortie)
{
/*
	Présentation : Cette fonction nous permet de compter le nombre de passage d'une abeille dans une porte. Les variables
	etant communes au programme la valeur "entree' et "sortie" compte les entrées sorties de toutes les abeilles de toutes
	les portes
	Explications:
	1- Nous determinloopons le sens detime_sleep déplacement des abeilles 
	2- Nous regardons dans quelles zones elles sont et distingons 3 cas (dans la zone "entrée", dans la zone"sortie" et 
	dans la zone "rien"
	3- Esuite si le mouvement respecte la postition, nous determinons le cas dans lequel nous sommes.
	4- Enfin on detecte que l'abeille quitte bien la zone de détection pour eviter un comptage inutile
	5- Pour finir, nous enregistrons la derniere position de l'abeille pour determiner ensuite son nouveau mouvement
	Précisions: 
	C'est comme se servir d'un vecteur ou nous cherchons a detecter sa direction, son amplitude n'ayant aucun effet. 
*/
	//Je détermine le sens de l'abeille  (-1 sortie 1 entree 0indeterminé) l'état 0 n'est présent qu'apres un comptage
	if(delimiteur[i][0]==0 && delimiteur[i][1]==1)
	{
		flagSens[i] = 2;
	}
	
	if(delimiteur[i][0]==1 && delimiteur[i][1]==0)
	{
		flagSens[i] = 1;
	}
	
	// on regarde si l'abeille un bien passé la limite
	if(delimiteur[i][0]==1 && delimiteur[i][1]==1)
	{
		flagAccept[i] = 1;
	}
	
	//si elle a passé la limite et qu'elle n'est plus présente dans le couloir de détection, cette dernière est comptabilisée
	if(delimiteur[i][0]==0 && delimiteur[i][1]==0 && flagAccept[i]==1)
	{
		if(flagSens[i]==1)
		{
			*entree=*entree+1;
			CumulEntree++;
			flagSens[i]=0;
			flagAccept[i]=0;
		}
		else if(flagSens[i]==2)
		{
			*sortie=*sortie+1;
			CumulSortie++;
			flagSens[i]=0;
			flagAccept[i]=0;
		}
	}
	
	return;
}

void recup_fond_continu(void)
{
	int i=0;
	for(i=0;i<nombreporte;i++)
	{
		if(delimiteur[i][0]==0 && delimiteur[i][1]==0)
		{
			bg[i](source(Rect(X[0],Y[i*2],X[1]-X[0],Y[i*2+1]-Y[i*2])), foreground[i]);
			bg[i].getBackgroundImage(fond[i]); // Récupération du fond obtenu par l'aquisition précédente
			cvtColor(fond[i],fond[i],CV_BGR2GRAY);  // on passe en nuance de gris (facilité de détection)
		}
	}
}	

void recup_fond(void) //OK
{
	int aqui=0;
	int i=0;
	printf("Aquisition du fond .... \n");
	while (capture.read(source) && aqui <= 1000)
	{
		for(i=0;i<nombreporte;i++)
		{
			//on récupère le fond de la zone qui nous interesse (partie gauche))
			bg[i](source(Rect(X[0],Y[i*2],X[1]-X[0],Y[i*2+1]-Y[i*2])), foreground[i]); 	
			
		}
		waitKey(1);
		aqui++;
		//30secondes environ
		if(aqui%100 == 0)
		{
			printf("...\n");
		}
		
	}
	for(i=0;i<nombreporte;i++)
	{
		bg[i].getBackgroundImage(fond[i]); // Récupération du fond obtenu par l'aquisition précédente
		cvtColor(fond[i],fond[i],CV_BGR2GRAY);  // on passe en nuance de gris (facilité de détection)		
	}
	printf("Fin aquisition!\n");
}

int main(void)
{

	// clock_t start, end;
	// int boucle=1;
	int i=0;
	char Affichage[50]={0};
	capture.set(CV_CAP_PROP_FRAME_WIDTH,640);
	capture.set(CV_CAP_PROP_FRAME_HEIGHT,480);

	if(!capture.isOpened())
	{
		printf("Impossible d'initialiser le flux video\nVerifiez les branchements\n");
		return -1; 
	}
	// cette boucle While récupère une multitude d'image pour obtenir un fond du passage des abeilles.
	// En récupérant plusieurs images, elle peut ainsi supprimer les abeilles qui passent dans le champ de vision et obtenir un bon résultat.
	
	calib_auto(); // fonction commentée permettant de récupérere les passages de facon automatique.
	calibration(source,2); // récupération des zones de passage ("1" si on veut l'affichage autre chose sinon)
	// après cette fonction, nous récupérons seulement une partie de l'image. pour faciliter les traitements
	
		
	recup_fond();
		
	
	
	while(capture.read(source))
	{
		//on commence par récupérer l'heure pour l'afficher
		get_time();
		
		//on garde seulement la partie qui nous interesse et qui est de la meme dimension que la matrice de référence
		sourcecrop = source(Rect(X[0],Y[0],X[1]-X[0],Y[21]-Y[0]));		
		//on colore la zone récupérée en gris (pour effectuer la différence entre le fond et le flux d'image
		cvtColor(sourcecrop,sourcegray,CV_BGR2GRAY); //convertion en gris
		//imshow("sourcegray",sourcegray);
		///---------------
		
		// Ici nous récupérons les 11 zones de détection et effectuons en plus la treshold pour obtenir la tache blanc sur noir que représente l'abeille.
		//------------------
		for(i=0;i<nombreporte;i++)
		{
			source_crop[i] = sourcegray(Rect(0,Y[i*2]-Y[0],X[1]-X[0],Y[i*2+1]-Y[i*2]));//Découpe l'image en portions ( "nombreporte" au total)
			absdiff(fond[i],source_crop[i],detection[i]); // detection des différences entre l'image de référence et l'image recue
			threshold(detection[i],canny_out[i],40,255,THRESH_BINARY);//effectue le threshold entre les deux images (flux et fond) retourne une imaga noir et blanc où l'abeille est récupérée comme un point blanc.
			suppressbruit(canny_out[i]); //suppression du bruit
		}	
		
		//------------------
		// Boucle effectuant le comptage
		//-------------------
		for(i=0;i<nombreporte;i++)
		{
			Comptage[i] = canny_out[i](Rect(0,0,X[2]-X[0],Y[i*2+1]-Y[i*2])); //on ne prend que la partie de gauche
			ColorZone(i,Comptage[i],posXTotal,posYTotal,LastXTotal,LastYTotal,1);// determine si il y a présence ou non d'une abeille du coté gauche (coté ruche)			
			Comptage[i] = canny_out[i](Rect(X[2]-X[0],0,X[2]-X[0],Y[i*2+1]-Y[i*2])); //on ne prend que la partie de droite
			ColorZone(i,Comptage[i],posXTotal,posYTotal,LastXTotal,LastYTotal,2);// détermine si il y a presence ou non d'une abeille du côté droit (côté extérieur)
			passage(i,deplacementTotal,bisXTotal,LastXTotal,flagTotal,&Entree,&Sortie);	// Partie comptage. explications dans la fonction	
		}		
		
		// Cette fonction permet de récupérer le fond de chaque passage en fonction de l'état de la porte.
		recup_fond_continu();
		
		// Affichage des données sur le flux vidéo
		sprintf(Affichage,"1: %d %d 2:%d %d 3: %d %d 4: %d %d 5: %d %d 6: %d %d ",delimiteur[0][0],delimiteur[0][1],delimiteur[1][0],delimiteur[1][1],delimiteur[2][0],delimiteur[2][1],delimiteur[3][0],delimiteur[3][1],delimiteur[4][0],delimiteur[4][1],delimiteur[5][0],delimiteur[5][1]);
		putText(source,Affichage,Point(20,20),FONT_HERSHEY_SIMPLEX,0.7,Scalar(255,0,0));
		sprintf(Affichage,"7: %d %d 8:%d %d 9: %d %d 10: %d %d 11: %d %d ",delimiteur[6][0],delimiteur[6][1],delimiteur[7][0],delimiteur[7][1],delimiteur[8][0],delimiteur[8][1],delimiteur[9][0],delimiteur[9][1],delimiteur[10][0],delimiteur[10][1]);
		putText(source,Affichage,Point(20,50),FONT_HERSHEY_SIMPLEX,0.7,Scalar(255,0,0));
		imshow("stream",source);
		waitKey(1);//dure 8ms normalement que 1ms <- normal, cette fonction attends au moins 1ms 
	} 
	
	printf("bonsoir\n");
	return 0;
}