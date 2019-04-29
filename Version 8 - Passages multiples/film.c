// g++ -Wall -o camera camera.c -lrt ` pkg-config --cflags --libs opencv `
/*sed -i ‘s/if (DEFINED CMAKE_TOOLCHAIN_FILE)/if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)/g’ makefiles/cmake/arm-linux.cmake*/

#include "opencv2/core/core.hpp" //bibliotheque générale d'opencv
#include "opencv2/highgui/highgui.hpp" //bibilotheque auxilliaire(traitement d'image)
#include "opencv2/imgproc/imgproc.hpp" //bibliotheque auxilliaire(affichage des images)
#include "opencv2/opencv.hpp" // Root des bibilotheques
//#include "opencv2/gpu/gpu.hpp"
#include <stdlib.h>

#include <stdio.h>

#include <iostream> //bibliotheque de gestion des entrées video
#include <string.h>
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
VideoCapture capture(-1); //initialisation du flux(on le met ici car toutes les fonctions profiterons du flux video sans redéclaration
VideoWriter outputVideo;
int main(void)
{

	// clock_t start, end;
	// int boucle=1;
	int image=0;

	if(!capture.isOpened())
	{
		printf("Impossible d'initialiser le flux video\nVerifiez les branchements\n");
		return -1; 
	}
	//int ex = static_cast<int>(capture.get(CV_CAP_PROP_FOURCC));
	Size S=Size((int) capture.get(CV_CAP_PROP_FRAME_WIDTH),(int) capture.get(CV_CAP_PROP_FRAME_HEIGHT));//Récupération de la taille de la video
	
	outputVideo.open("Passage_20_09_17_1.mpeg",CV_FOURCC('P','I','M','1'), 30, S, 1);
	
	if (!outputVideo.isOpened())
    {
        printf("Fichier de retour non initialisé\n");
        return -1;
    }

	while(capture.read(source) && image <30000)
	{
		imshow("stream",source);
		
		outputVideo.write(source);
		image++;
		waitKey(1);//dure 8ms normalement que 1ms <- normal, cette fonction attends au moins 1ms 
	} 
	
	printf("bonsoir\n");
	return 0;
}