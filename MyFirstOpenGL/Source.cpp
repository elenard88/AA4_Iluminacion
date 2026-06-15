#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <gtc/matrix_transform.hpp>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <stb_image.h>
#include "Model.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 1024

const float YAW = -90.f;
const float PITCH = -8.f;
const float SPEED = 4.f;
const float SENSITIVITY = 0.1f;

const float CAMERA_MIN_PITCH = -89.f;
const float CAMERA_MAX_PITCH = 89.f;

const float FIRST_MOUSE_X = WINDOW_WIDTH * 0.5f;
const float FIRST_MOUSE_Y = WINDOW_HEIGHT * 0.5f;

struct ShaderProgram {
	GLuint vertexShader = 0;
	GLuint geometryShader = 0;
	GLuint fragmentShader = 0;
};

struct GameObject {
	unsigned int modelIndex = 0;
	unsigned int textureIndex = 0;

	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 rotation = glm::vec3(0.f);
	glm::vec3 scale = glm::vec3(1.f);
};

enum Camera_Movement {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT
};

struct Camera {
	glm::vec3 Position = glm::vec3(0.f, 2.f, 12.f);
	glm::vec3 Front = glm::vec3(0.f, 0.f, -1.f);
	glm::vec3 Up = glm::vec3(0.f, 1.f, 0.f);
	glm::vec3 Right = glm::vec3(1.f, 0.f, 0.f);
	glm::vec3 WorldUp = glm::vec3(0.f, 1.f, 0.f);

	float Yaw = YAW;
	float Pitch = PITCH;

	float MovementSpeed = SPEED;
	float MouseSensitivity = SENSITIVITY;

	float LastMouseX = FIRST_MOUSE_X;
	float LastMouseY = FIRST_MOUSE_Y;

	bool FirstMouseInput = true;
};

std::vector<GLuint> compiledPrograms;
std::vector<Model> models;
std::vector<GLuint> textures;
std::vector<GameObject> gameObjects;

Camera camera;

float deltaTime = 0.f;
float lastFrameTime = 0.f;

const unsigned int MODEL_TROLL = 0;
const unsigned int MODEL_ROCK = 1;
const unsigned int MODEL_PALM = 2;

void Resize_Window(GLFWwindow* window, int iFrameBufferWidth, int iFrameBufferHeight) {

	//Definir nuevo tamaño del viewport
	glViewport(0, 0, iFrameBufferWidth, iFrameBufferHeight);
	glUniform2f(glGetUniformLocation(compiledPrograms[0], "windowSize"), iFrameBufferWidth, iFrameBufferHeight);
}

//Funcion que leera un .obj y devolvera un modelo para poder ser renderizado
Model LoadOBJModel(const std::string& filePath) {

	//Verifico archivo y si no puedo abrirlo cierro aplicativo
	std::ifstream file(filePath);

	if (!file.is_open()) {
		std::cerr << "No se ha podido abrir el archivo: " << filePath << std::endl;
		std::exit(EXIT_FAILURE);
	}

	//Variables lectura fichero
	std::string line;
	std::stringstream ss;
	std::string prefix;
	glm::vec3 tmpVec3;
	glm::vec2 tmpVec2;

	//Variables elemento modelo
	std::vector<float> vertexs;
	std::vector<float> vertexNormal;
	std::vector<float> textureCoordinates;

	//Variables temporales para algoritmos de sort
	std::vector<float> tmpVertexs;
	std::vector<float> tmpNormals;
	std::vector<float> tmpTextureCoordinates;

	//Recorremos archivo linea por linea
	while (std::getline(file, line)) {

		//Por cada linea reviso el prefijo del archivo que me indica que estoy analizando
		ss.clear();
		ss.str(line);
		ss >> prefix;

		//Estoy leyendo un vertice
		if (prefix == "v") {

			//Asumo que solo trabajo 3D así que almaceno XYZ de forma consecutiva
			ss >> tmpVec3.x >> tmpVec3.y >> tmpVec3.z;

			//Almaceno en mi vector de vertices los valores
			tmpVertexs.push_back(tmpVec3.x);
			tmpVertexs.push_back(tmpVec3.y);
			tmpVertexs.push_back(tmpVec3.z);
		}

		//Estoy leyendo una UV (texture coordinate)
		else if (prefix == "vt") {

			//Las UVs son siempre imagenes 2D asi que uso el tmpvec2 para almacenarlas
			ss >> tmpVec2.x >> tmpVec2.y;

			//Almaceno en mi vector temporal las UVs
			tmpTextureCoordinates.push_back(tmpVec2.x);
			tmpTextureCoordinates.push_back(tmpVec2.y);

		}

		//Estoy leyendo una normal
		else if (prefix == "vn") {

			//Asumo que solo trabajo 3D así que almaceno XYZ de forma consecutiva
			ss >> tmpVec3.x >> tmpVec3.y >> tmpVec3.z;

			//Almaceno en mi vector temporal de normales las normales
			tmpNormals.push_back(tmpVec3.x);
			tmpNormals.push_back(tmpVec3.y);
			tmpNormals.push_back(tmpVec3.z);

		}

		//Estoy leyendo una cara
		else if (prefix == "f") {

			int vertexData;
			short counter = 0;

			//Obtengo todos los valores hasta un espacio
			while (ss >> vertexData) {

				//En orden cada numero sigue el patron de vertice/uv/normal
				switch (counter) {
				case 0:
					//Si es un vertice lo almaceno - 1 por el offset y almaceno dos seguidos al ser un vec3, salto 1 / y aumento el contador en 1
					vertexs.push_back(tmpVertexs[(vertexData - 1) * 3]);
					vertexs.push_back(tmpVertexs[((vertexData - 1) * 3) + 1]);
					vertexs.push_back(tmpVertexs[((vertexData - 1) * 3) + 2]);
					ss.ignore(1, '/');
					counter++;
					break;
				case 1:
					//Si es un uv lo almaceno - 1 por el offset y almaceno dos seguidos al ser un vec2, salto 1 / y aumento el contador en 1
					textureCoordinates.push_back(tmpTextureCoordinates[(vertexData - 1) * 2]);
					textureCoordinates.push_back(tmpTextureCoordinates[((vertexData - 1) * 2) + 1]);
					ss.ignore(1, '/');
					counter++;
					break;
				case 2:
					//Si es una normal la almaceno - 1 por el offset y almaceno tres seguidos al ser un vec3, salto 1 / y reinicio
					vertexNormal.push_back(tmpNormals[(vertexData - 1) * 3]);
					vertexNormal.push_back(tmpNormals[((vertexData - 1) * 3) + 1]);
					vertexNormal.push_back(tmpNormals[((vertexData - 1) * 3) + 2]);
					counter = 0;
					break;
				}
			}
		}
	}
	return Model(vertexs, textureCoordinates, vertexNormal);
}

//Funcion que genera un numero decimal aleatorio entre un minimo y un maximo
float GenerateRandomFloat(float minValue, float maxValue) {

	float randomValue = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	return minValue + randomValue * (maxValue - minValue);
}

//Funcion que genera una escala coherente segun el modelo
glm::vec3 GenerateRandomScale(unsigned int modelIndex) {

	float randomScale = 1.f;

	switch (modelIndex) {
	case MODEL_TROLL:
		randomScale = GenerateRandomFloat(0.3f, 0.5f);
		break;

	case MODEL_ROCK:
		randomScale = GenerateRandomFloat(0.8f, 1.6f);
		break;

	case MODEL_PALM:
		randomScale = GenerateRandomFloat(0.7f, 1.2f);
		break;

	default:
		randomScale = 1.f;
		break;
	}

	return glm::vec3(randomScale);
}

//genera la matriz de rotacion a partir de una rotacion en grados
glm::mat4 GenerateRotationMatrix(const glm::vec3& rotation) {

	glm::mat4 rotationMatrix = glm::mat4(1.f);

	rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.x), glm::vec3(1.f, 0.f, 0.f));
	rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.y), glm::vec3(0.f, 1.f, 0.f));
	rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.z), glm::vec3(0.f, 0.f, 1.f));

	return rotationMatrix;
}

//Funcion que actualiza los vectores de direccion de la camara
void UpdateCameraVectors() {

	glm::vec3 front;

	front.x = cos(glm::radians(camera.Yaw)) * cos(glm::radians(camera.Pitch));
	front.y = sin(glm::radians(camera.Pitch));
	front.z = sin(glm::radians(camera.Yaw)) * cos(glm::radians(camera.Pitch));

	camera.Front = glm::normalize(front);
	camera.Right = glm::normalize(glm::cross(camera.Front, camera.WorldUp));
	camera.Up = glm::normalize(glm::cross(camera.Right, camera.Front));
}

//Funcion que devuelve la matriz de vista de la camara
glm::mat4 GetViewMatrix() {

	return glm::lookAt(camera.Position, camera.Position + camera.Front, camera.Up);
}

//Funcion que procesa movimiento de teclado de la camara
void ProcessKeyboard(Camera_Movement direction) {

	float velocity = camera.MovementSpeed * deltaTime;

	if (direction == FORWARD) {
		camera.Position += camera.Front * velocity;
	}

	if (direction == BACKWARD) {
		camera.Position -= camera.Front * velocity;
	}

	if (direction == RIGHT) {
		camera.Position += camera.Right * velocity;
	}

	if (direction == LEFT) {
		camera.Position -= camera.Right * velocity;
	}
}

//Funcion que procesa el movimiento del raton
void ProcessMouseMovement(float xOffset, float yOffset) {

	xOffset *= camera.MouseSensitivity;
	yOffset *= camera.MouseSensitivity;

	camera.Yaw += xOffset;
	camera.Pitch += yOffset;

	if (camera.Pitch > CAMERA_MAX_PITCH) {
		camera.Pitch = CAMERA_MAX_PITCH;
	}

	if (camera.Pitch < CAMERA_MIN_PITCH) {
		camera.Pitch = CAMERA_MIN_PITCH;
	}

	UpdateCameraVectors();
}

//Funcion que procesa inputs de teclado mediante GLFW
void ProcessInput(GLFWwindow* window) {

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		ProcessKeyboard(FORWARD);
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		ProcessKeyboard(BACKWARD);
	}

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		ProcessKeyboard(LEFT);
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		ProcessKeyboard(RIGHT);
	}

	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, true);
	}
}

//Funcion callback que procesa el movimiento del raton
void Mouse_Callback(GLFWwindow* window, double mouseX, double mouseY) {

	if (camera.FirstMouseInput) {
		camera.LastMouseX = static_cast<float>(mouseX);
		camera.LastMouseY = static_cast<float>(mouseY);
		camera.FirstMouseInput = false;
	}

	float xOffset = static_cast<float>(mouseX) - camera.LastMouseX;
	float yOffset = camera.LastMouseY - static_cast<float>(mouseY);

	camera.LastMouseX = static_cast<float>(mouseX);
	camera.LastMouseY = static_cast<float>(mouseY);

	ProcessMouseMovement(xOffset, yOffset);
}

//Carga una textura y devuelve su id
GLuint LoadTexture(const std::string& filePath) {

	int width = 0;
	int height = 0;
	int nrChannels = 0;

	unsigned char* textureInfo = stbi_load(filePath.c_str(), &width, &height, &nrChannels, 0);

	if (textureInfo == nullptr) {
		std::cerr << "No se ha podido cargar la textura: " << filePath << std::endl;
		std::exit(EXIT_FAILURE);
	}

	GLenum textureFormat = GL_RGB;

	if (nrChannels == 4) {
		textureFormat = GL_RGBA;
	}

	GLuint textureID;
	glGenTextures(1, &textureID);

	glBindTexture(GL_TEXTURE_2D, textureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, textureFormat, width, height, 0, textureFormat, GL_UNSIGNED_BYTE, textureInfo);
	glGenerateMipmap(GL_TEXTURE_2D);

	stbi_image_free(textureInfo);

	return textureID;
}

//Funcion que genera los objetos de la escena en spawnpoints
void GenerateSceneObjects() {

	gameObjects.clear();

	std::vector<glm::vec3> spawnPoints = { //puntos hechos con IA
	glm::vec3(-5.f, 0.f, -5.f),
	glm::vec3(0.f, 0.f, -5.f),
	glm::vec3(5.f, 0.f, -5.f),

	glm::vec3(-5.f, 0.f, 0.f),
	glm::vec3(0.f, 0.f, 0.f),
	glm::vec3(5.f, 0.f, 0.f),

	glm::vec3(-5.f, 0.f, 5.f),
	glm::vec3(0.f, 0.f, 5.f),
	glm::vec3(5.f, 0.f, 5.f)
	};

	for (unsigned int i = 0; i < spawnPoints.size(); i++) {

		GameObject gameObject;

		if (i < 3) {
			gameObject.modelIndex = i;
		}
		else {
			gameObject.modelIndex = rand() % models.size();
		}

		gameObject.textureIndex = gameObject.modelIndex;
		gameObject.position = spawnPoints[i];
		gameObject.rotation = glm::vec3(0.f, GenerateRandomFloat(0.f, 360.f), 0.f);
		gameObject.scale = GenerateRandomScale(gameObject.modelIndex);

		gameObjects.push_back(gameObject);
	}
}


//Funcion que devolvera una string con todo el archivo leido
std::string Load_File(const std::string& filePath) {

	std::ifstream file(filePath);

	std::string fileContent;
	std::string line;

	//Lanzamos error si el archivo no se ha podido abrir
	if (!file.is_open()) {
		std::cerr << "No se ha podido abrir el archivo: " << filePath << std::endl;
		std::exit(EXIT_FAILURE);
	}

	//Leemos el contenido y lo volcamos a la variable auxiliar
	while (std::getline(file, line)) {
		fileContent += line + "\n";
	}

	//Cerramos stream de datos y devolvemos contenido
	file.close();

	return fileContent;
}

GLuint LoadFragmentShader(const std::string& filePath) {

	// Crear un fragment shader
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	//Usamos la funcion creada para leer el fragment shader y almacenarlo 
	std::string sShaderCode = Load_File(filePath);
	const char* cShaderSource = sShaderCode.c_str();

	//Vinculamos el fragment shader con su código fuente
	glShaderSource(fragmentShader, 1, &cShaderSource, nullptr);

	// Compilar el fragment shader
	glCompileShader(fragmentShader);

	// Verificar errores de compilación
	GLint success;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);

	//Si la compilacion ha sido exitosa devolvemos el fragment shader
	if (success) {

		return fragmentShader;

	}
	else {

		//Obtenemos longitud del log
		GLint logLength;
		glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &logLength);

		//Obtenemos el log
		std::vector<GLchar> errorLog(logLength);
		glGetShaderInfoLog(fragmentShader, logLength, nullptr, errorLog.data());

		//Mostramos el log y finalizamos programa
		std::cerr << "Se ha producido un error al cargar el fragment shader:  " << errorLog.data() << std::endl;
		std::exit(EXIT_FAILURE);
	}
}


GLuint LoadGeometryShader(const std::string& filePath) {

	// Crear un vertex shader
	GLuint geometryShader = glCreateShader(GL_GEOMETRY_SHADER);

	//Usamos la funcion creada para leer el vertex shader y almacenarlo 
	std::string sShaderCode = Load_File(filePath);
	const char* cShaderSource = sShaderCode.c_str();

	//Vinculamos el vertex shader con su código fuente
	glShaderSource(geometryShader, 1, &cShaderSource, nullptr);

	// Compilar el vertex shader
	glCompileShader(geometryShader);

	// Verificar errores de compilación
	GLint success;
	glGetShaderiv(geometryShader, GL_COMPILE_STATUS, &success);

	//Si la compilacion ha sido exitosa devolvemos el vertex shader
	if (success) {

		return geometryShader;

	}
	else {

		//Obtenemos longitud del log
		GLint logLength;
		glGetShaderiv(geometryShader, GL_INFO_LOG_LENGTH, &logLength);

		//Obtenemos el log
		std::vector<GLchar> errorLog(logLength);
		glGetShaderInfoLog(geometryShader, logLength, nullptr, errorLog.data());

		//Mostramos el log y finalizamos programa
		std::cerr << "Se ha producido un error al cargar el vertex shader:  " << errorLog.data() << std::endl;
		std::exit(EXIT_FAILURE);
	}
}

GLuint LoadVertexShader(const std::string& filePath) {

	// Crear un vertex shader
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);

	//Usamos la funcion creada para leer el vertex shader y almacenarlo 
	std::string sShaderCode = Load_File(filePath);
	const char* cShaderSource = sShaderCode.c_str();

	//Vinculamos el vertex shader con su código fuente
	glShaderSource(vertexShader, 1, &cShaderSource, nullptr);

	// Compilar el vertex shader
	glCompileShader(vertexShader);

	// Verificar errores de compilación
	GLint success;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);

	//Si la compilacion ha sido exitosa devolvemos el vertex shader
	if (success) {

		return vertexShader;

	}
	else {

		//Obtenemos longitud del log
		GLint logLength;
		glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &logLength);

		//Obtenemos el log
		std::vector<GLchar> errorLog(logLength);
		glGetShaderInfoLog(vertexShader, logLength, nullptr, errorLog.data());

		//Mostramos el log y finalizamos programa
		std::cerr << "Se ha producido un error al cargar el vertex shader:  " << errorLog.data() << std::endl;
		std::exit(EXIT_FAILURE);
	}
}

//Función que dado un struct que contiene los shaders de un programa generara el programa entero de la GPU
GLuint CreateProgram(const ShaderProgram& shaders) {

	//Crear programa de la GPU
	GLuint program = glCreateProgram();

	//Verificar que existe un vertex shader y adjuntarlo al programa
	if (shaders.vertexShader != 0) {
		glAttachShader(program, shaders.vertexShader);
	}

	if (shaders.geometryShader != 0) {
		glAttachShader(program, shaders.geometryShader);
	}

	if (shaders.fragmentShader != 0) {
		glAttachShader(program, shaders.fragmentShader);
	}

	// Linkear el programa
	glLinkProgram(program);

	//Obtener estado del programa
	GLint success;
	glGetProgramiv(program, GL_LINK_STATUS, &success);

	//Devolver programa si todo es correcto o mostrar log en caso de error
	if (success) {

		//Liberamos recursos
		if (shaders.vertexShader != 0) {
			glDetachShader(program, shaders.vertexShader);
		}

		//Liberamos recursos
		if (shaders.geometryShader != 0) {
			glDetachShader(program, shaders.geometryShader);
		}

		//Liberamos recursos
		if (shaders.fragmentShader != 0) {
			glDetachShader(program, shaders.fragmentShader);
		}

		return program;
	}
	else {

		//Obtenemos longitud del log
		GLint logLength;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

		//Almacenamos log
		std::vector<GLchar> errorLog(logLength);
		glGetProgramInfoLog(program, logLength, nullptr, errorLog.data());

		std::cerr << "Error al linkar el programa:  " << errorLog.data() << std::endl;
		std::exit(EXIT_FAILURE);
	}
}

void main() {

	//Definir semillas del rand según el tiempo
	srand(static_cast<unsigned int>(time(NULL)));

	//Inicializamos GLFW para gestionar ventanas e inputs
	glfwInit();

	//Configuramos la ventana
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

	//Inicializamos la ventana
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "My Engine", NULL, NULL);

	//Asignamos función de callback para cuando el frame buffer es modificado
	glfwSetFramebufferSizeCallback(window, Resize_Window);

	//Asignamos funcion de callback para el movimiento del raton
	glfwSetCursorPosCallback(window, Mouse_Callback);

	//Capturamos el cursor dentro de la ventana
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	//Definimos espacio de trabajo
	glfwMakeContextCurrent(window);

	//Permitimos a GLEW usar funcionalidades experimentales
	glewExperimental = GL_TRUE;

	//Activamos cull face
	glEnable(GL_CULL_FACE);

	//Indicamos lado del culling
	glCullFace(GL_BACK);

	//Indicamos lado del culling
	glEnable(GL_DEPTH_TEST);

	

	//Inicializamos GLEW y controlamos errores
	if (glewInit() == GLEW_OK) {

		//Compilar shaders
		ShaderProgram myFirstProgram;
		myFirstProgram.vertexShader = LoadVertexShader("MyFirstVertexShader.glsl");
		myFirstProgram.geometryShader = LoadGeometryShader("MyFirstGeometryShader.glsl");
		myFirstProgram.fragmentShader = LoadFragmentShader("MyFirstFragmentShader.glsl");

		//Cargo Modelo
		models.push_back(LoadOBJModel("Assets/Models/troll.obj"));
		models.push_back(LoadOBJModel("Assets/Models/rock.obj"));
		models.push_back(LoadOBJModel("Assets/Models/palm.obj"));

		//Compìlar programa
		compiledPrograms.push_back(CreateProgram(myFirstProgram));

		//Definimos canal de textura activo
		glActiveTexture(GL_TEXTURE0);

		//Cargo texturas
		textures.push_back(LoadTexture("Assets/Textures/troll.png"));
		textures.push_back(LoadTexture("Assets/Textures/rock.png"));
		textures.push_back(LoadTexture("Assets/Textures/palm.png"));

		//Genero objetos de escena
		GenerateSceneObjects();

		//Definimos color para limpiar el buffer de color
		glClearColor(0.f, 0.f, 0.f, 1.f);

		//Definimos modo de dibujo para cada cara
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		//Indicar a la tarjeta GPU que programa debe usar
		glUseProgram(compiledPrograms[0]);

		//Definir la matriz de traslacion, rotacion y escalado
		glm::mat4 translationMatrix = glm::translate(glm::mat4(1.f), glm::vec3(0.f));
		glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.f), glm::vec3(1.f));

		UpdateCameraVectors();

		//Definir la matriz de vista
		glm::mat4 view = GetViewMatrix();

		// Definir la matriz proyeccion
		glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 100.0f);

		//Asignar valores iniciales al programa
		glUniform2f(glGetUniformLocation(compiledPrograms[0], "windowSize"), WINDOW_WIDTH, WINDOW_HEIGHT);

		//Asignar valor variable de textura a usar.
		glUniform1i(glGetUniformLocation(compiledPrograms[0], "textureSampler"), 0);

		//Obtenemos referencias a las variables uniform
		GLint translationMatrixReference = glGetUniformLocation(compiledPrograms[0], "translationMatrix");
		GLint rotationMatrixReference = glGetUniformLocation(compiledPrograms[0], "rotationMatrix");
		GLint scaleMatrixReference = glGetUniformLocation(compiledPrograms[0], "scaleMatrix");
		GLint viewReference = glGetUniformLocation(compiledPrograms[0], "view");
		GLint projectionReference = glGetUniformLocation(compiledPrograms[0], "projection");

		// Pasar las matrices
		glUniformMatrix4fv(viewReference, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(projectionReference, 1, GL_FALSE, glm::value_ptr(projection));

		//Generamos el game loop
		while (!glfwWindowShouldClose(window)) {

			//Calculamos delta time
			float currentFrameTime = static_cast<float>(glfwGetTime());
			deltaTime = currentFrameTime - lastFrameTime;
			lastFrameTime = currentFrameTime;

			//Pulleamos los eventos (botones, teclas, mouse...)
			glfwPollEvents();

			//Procesamos los inputs de camara
			ProcessInput(window);

			//Actualizamos la matriz de vista
			view = GetViewMatrix();
			glUniformMatrix4fv(viewReference, 1, GL_FALSE, glm::value_ptr(view));
			
			//Limpiamos los buffers
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

			//Renderizo todos los objetos de la escena
			for (unsigned int i = 0; i < gameObjects.size(); i++) {

				const GameObject& gameObject = gameObjects[i];

				glm::mat4 translationMatrix = glm::translate(glm::mat4(1.f), gameObject.position);
				glm::mat4 rotationMatrix = GenerateRotationMatrix(gameObject.rotation);
				glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.f), gameObject.scale);

				glUniformMatrix4fv(translationMatrixReference, 1, GL_FALSE, glm::value_ptr(translationMatrix));
				glUniformMatrix4fv(rotationMatrixReference, 1, GL_FALSE, glm::value_ptr(rotationMatrix));
				glUniformMatrix4fv(scaleMatrixReference, 1, GL_FALSE, glm::value_ptr(scaleMatrix));

				glBindTexture(GL_TEXTURE_2D, textures[gameObject.textureIndex]);

				models[gameObject.modelIndex].Render();
			}

			//Cambiamos buffers
			glFlush();
			glfwSwapBuffers(window);
		}

		//Desactivar y eliminar programa
		glUseProgram(0);
		glDeleteProgram(compiledPrograms[0]);

	}
	else {
		std::cout << "Ha petao." << std::endl;
		glfwTerminate();
	}

	//Finalizamos GLFW
	glfwTerminate();

}