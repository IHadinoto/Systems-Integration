#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <map>
#include <SMObject.h>
#include <SMStruct.h>
#using <System.dll>

using namespace System;
using namespace System::Diagnostics;
using namespace System::Net::Sockets;
using namespace System::Net;
using namespace System::Text;

#ifdef __APPLE__
	#include <OpenGL/gl.h>
	#include <OpenGL/glu.h>
	#include <GLUT/glut.h>
	#include <unistd.h>
	#include <sys/time.h>
#elif defined(WIN32)
	#include <Windows.h>
	#include <tchar.h>
	#include <GL/gl.h>
	#include <GL/glu.h>
	#include <GL/glut.h>
#else
	#include <GL/gl.h>
	#include <GL/glu.h>
	#include <GL/glut.h>
	#include <unistd.h>
	#include <sys/time.h>
#endif


#include "Camera.hpp"
#include "Ground.hpp"
#include "KeyManager.hpp"

#include "Shape.hpp"
#include "Vehicle.hpp"
#include "MyVehicle.hpp"

#include "Messages.hpp"
#include "HUD.hpp"

using namespace scos;

void display();
void reshape(int width, int height);
void idle();

void keydown(unsigned char key, int x, int y);
void keyup(unsigned char key, int x, int y);
void special_keydown(int keycode, int x, int y);
void special_keyup(int keycode, int x, int y);

void mouse(int button, int state, int x, int y);
void dragged(int x, int y);
void motion(int x, int y);

void drawLaser();
void drawGPS();
void addLine(double x, double y);

#define WAIT_TIME 500

// Used to store the previous mouse location so we
//   can calculate relative mouse movement.
int prev_mouse_x = -1;
int prev_mouse_y = -1;

// vehicle control related variables
Vehicle * vehicle = NULL;
double speed = 0;
double steering = 0;

// In global scope, declare shared memory
SMObject GalilSMObj(_TEXT("GalilSMObject"), sizeof(SMData));
SMObject PMSMObj(_TEXT("PMSMObj"), sizeof(PM));

//int _tmain(int argc, _TCHAR* argv[]) {
int main(int argc, char ** argv) {
	// Instantiate shared memory and structs
	// SM Stuff
	GalilSMObj.SMAccess();
	// PM Stuff
	PMSMObj.SMAccess();

	const int WINDOW_WIDTH = 800;
	const int WINDOW_HEIGHT = 600;

	glutInit(&argc, (char**)(argv));
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	glutCreateWindow("MTRN3500 - GL");

	Camera::get()->setWindowDimensions(WINDOW_WIDTH, WINDOW_HEIGHT);

	glEnable(GL_DEPTH_TEST);

	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutIdleFunc(idle);

	glutKeyboardFunc(keydown);
	glutKeyboardUpFunc(keyup);
	glutSpecialFunc(special_keydown);
	glutSpecialUpFunc(special_keyup);

	glutMouseFunc(mouse);
	glutMotionFunc(dragged);
	glutPassiveMotionFunc(motion);

	// -------------------------------------------------------------------------
	// Please uncomment the following line of code and replace 'MyVehicle'
	//   with the name of the class you want to show as the current 
	//   custom vehicle.
	// -------------------------------------------------------------------------
	vehicle = new MyVehicle();


	glutMainLoop();

	
	if (vehicle != NULL) {
		delete vehicle;
	}
		
	return 0;
}


void display() {
	// -------------------------------------------------------------------------
	//  This method is the main draw routine. 
	// -------------------------------------------------------------------------

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if(Camera::get()->isPursuitMode() && vehicle != NULL) {
		double x = vehicle->getX(), y = vehicle->getY(), z = vehicle->getZ();
		double dx = cos(vehicle->getRotation() * 3.141592765 / 180.0);
		double dy = sin(vehicle->getRotation() * 3.141592765 / 180.0);
		Camera::get()->setDestPos(x + (-3 * dx), y + 7, z + (-3 * dy));
		Camera::get()->setDestDir(dx, -1, dy);
	}
	Camera::get()->updateLocation();
	Camera::get()->setLookAt();

	Ground::draw();
	
	// draw my vehicle
	if (vehicle != NULL) {
		vehicle->draw();

	}


	// draw HUD
	HUD::Draw();

	// draw Laser;
	drawLaser();
	// Draw GPS
	drawGPS();

	glutSwapBuffers();
};

void reshape(int width, int height) {

	Camera::get()->setWindowDimensions(width, height);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
};

double getTime()
{
#if defined(WIN32)
	LARGE_INTEGER freqli;
	LARGE_INTEGER li;
	if(QueryPerformanceCounter(&li) && QueryPerformanceFrequency(&freqli)) {
		return double(li.QuadPart) / double(freqli.QuadPart);
	}
	else {
		static ULONGLONG start = GetTickCount64();
		return (GetTickCount64() - start) / 1000.0;
	}
#else
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec + (t.tv_usec / 1000000.0);
#endif
}

void idle() {
	// Initialise Pointers
	PM* PMSMObjPtr = (PM*)PMSMObj.pData;
	SMData* GalilSMObjPtr = (SMData*)GalilSMObj.pData;

	// Heartbeat Stuff
	if (PMSMObjPtr->Shutdown.Flags.OpenGL) {
		exit(0);
	}

	// Heartbeat Checks
	if (PMSMObjPtr->Heartbeats.Flags.OpenGL == 0) {
		// Set Heartbeat Flag
		PMSMObjPtr->Heartbeats.Flags.OpenGL = 1;
		GalilSMObjPtr->OpenGL.PMCheck = 0;
	}
	// Check PM Heartbeat
	else {
		//Accumulate WaitAndSeeTime
		GalilSMObjPtr->OpenGL.PMCheck++;
		Console::Write("PM Check\t");
		Console::WriteLine(GalilSMObjPtr->OpenGL.PMCheck);
		if (GalilSMObjPtr->OpenGL.PMCheck > WAIT_TIME) {
			PMSMObjPtr->Shutdown.Flags.Vehicle = 1;
			System::Threading::Thread::Sleep(100);
			PMSMObjPtr->Shutdown.Flags.GPS = 1;
			System::Threading::Thread::Sleep(100);
			PMSMObjPtr->Shutdown.Flags.Camera = 1;
			System::Threading::Thread::Sleep(100);
			PMSMObjPtr->Shutdown.Flags.Laser = 1;
			System::Threading::Thread::Sleep(100);
			PMSMObjPtr->Shutdown.Flags.OpenGL = 1;
			System::Threading::Thread::Sleep(100);
		}
	}

	if (KeyManager::get()->isAsciiKeyPressed('a')) {
		Camera::get()->strafeLeft();
	}

	if (KeyManager::get()->isAsciiKeyPressed('c')) {
		Camera::get()->strafeDown();
	}

	if (KeyManager::get()->isAsciiKeyPressed('d')) {
		Camera::get()->strafeRight();
	}

	if (KeyManager::get()->isAsciiKeyPressed('s')) {
		Camera::get()->moveBackward();
	}

	if (KeyManager::get()->isAsciiKeyPressed('w')) {
		Camera::get()->moveForward();
	}

	if (KeyManager::get()->isAsciiKeyPressed(' ')) {
		Camera::get()->strafeUp();
	}

	speed = 0;
	steering = 0;

	if (KeyManager::get()->isSpecialKeyPressed(GLUT_KEY_LEFT)) {
		steering = Vehicle::MAX_LEFT_STEERING_DEGS * -1;   
	}

	if (KeyManager::get()->isSpecialKeyPressed(GLUT_KEY_RIGHT)) {
		steering = Vehicle::MAX_RIGHT_STEERING_DEGS * -1;
	}

	if (KeyManager::get()->isSpecialKeyPressed(GLUT_KEY_UP)) {
		speed = Vehicle::MAX_FORWARD_SPEED_MPS;
	}

	if (KeyManager::get()->isSpecialKeyPressed(GLUT_KEY_DOWN)) {
		speed = Vehicle::MAX_BACKWARD_SPEED_MPS;
	}

	const float sleep_time_between_frames_in_seconds = 0.025;

	static double previousTime = getTime();
	const double currTime = getTime();
	const double elapsedTime = currTime - previousTime;
	previousTime = currTime;

	// do a simulation step
	if (vehicle != NULL) {
		vehicle->update(speed, steering, elapsedTime);
	}


	display();

#ifdef _WIN32 
	Sleep(sleep_time_between_frames_in_seconds * 1000);
#else
	usleep(sleep_time_between_frames_in_seconds * 1e6);
#endif
};

void keydown(unsigned char key, int x, int y) {

	// keys that will be held down for extended periods of time will be handled
	//   in the idle function
	KeyManager::get()->asciiKeyPressed(key);

	// keys that react ocne when pressed rather than need to be held down
	//   can be handles normally, like this...
	switch (key) {
	case 27: // ESC key
		exit(0);
		break;      
	case '0':
		Camera::get()->jumpToOrigin();
		break;
	case 'p':
		Camera::get()->togglePursuitMode();
		break;
	}

};

void keyup(unsigned char key, int x, int y) {
	KeyManager::get()->asciiKeyReleased(key);
};

void special_keydown(int keycode, int x, int y) {

	KeyManager::get()->specialKeyPressed(keycode);

};

void special_keyup(int keycode, int x, int y) {  
	KeyManager::get()->specialKeyReleased(keycode);  
};

void mouse(int button, int state, int x, int y) {

};

void dragged(int x, int y) {

	if (prev_mouse_x >= 0) {

		int dx = x - prev_mouse_x;
		int dy = y - prev_mouse_y;

		Camera::get()->mouseRotateCamera(dx, dy);
	}

	prev_mouse_x = x;
	prev_mouse_y = y;
};

void motion(int x, int y) {

	prev_mouse_x = x;
	prev_mouse_y = y;
};

// Function for handling laser scans
void drawLaser() {
	SMData* GalilSMObjPtr = (SMData*)GalilSMObj.pData;

	glPushMatrix();
	vehicle->positionInGL();
	glTranslated(0.5, 0, 0); // move reference frame to lidar
	glLineWidth(2.5);
	glColor3f(1.0, 1.0, 1.0);
	glBegin(GL_LINES);

	for (int i = 0; i < GalilSMObjPtr->Laser.NumberRange; i++) {
		addLine(-GalilSMObjPtr->Laser.YCoordinate[i] / 1000, GalilSMObjPtr->Laser.XCoordinate[i] / 1000);
	}

	glEnd();
	glPopMatrix();
}

// Function for handling GPS Plots
void drawGPS() {
	SMData* GalilSMObjPtr = (SMData*)GalilSMObj.pData;

	glPushMatrix();
	//vehicle->positionInGL();
	glTranslated(-30, 0, 0); // move reference frame to GPS
	glLineWidth(2.5);
	glColor3f(0, 1.0, 1.0);
	glBegin(GL_LINES);

	for (int i = 0; i < GalilSMObjPtr->GPSP.NumberPoints; i++) {
		addLine(GalilSMObjPtr->GPSP.Northing[i], GalilSMObjPtr->GPSP.Easting[i]);
	}

	glEnd();
	glPopMatrix();
}

void addLine(double x, double y) {
	glVertex3f(y, 0.0, x);
	glVertex3f(y, 1.0, x);
}