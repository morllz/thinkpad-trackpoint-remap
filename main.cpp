#include <windows.h>
#include <chrono>  // For timing
#include <cmath>   // For std::abs
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

std::string trackpointDevicePath;
std::string hDevice;
bool isPreConfigured = false;
const int TOLERANCE = 5;        // Define tolerance threshold for movement detection
const int REST_TOLERANCE = 10;  // Tolerance to determine if the TrackPoint is essentially at (0,0)

// Variables to track the last known direction and position
bool movementRegistered = false;
std::chrono::steady_clock::time_point lastMovementTime;

// Define constants for time interval to prevent rapid repeated movements
const std::chrono::milliseconds MIN_INTERVAL(500);  // 500 ms delay between same movements

std::string hexify(HANDLE h) {
    std::stringstream ss;
    ss << std::hex << h;
    return ss.str();
}

void SendArrowKey(WORD key) {
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key;

    // Send key down event
    SendInput(1, &input, sizeof(INPUT));

    // Set up key up event
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

void setMovement(std::string devicePath, RAWINPUT* raw) {
    if (trackpointDevicePath == devicePath && hDevice == hexify(raw->header.hDevice)) {
        int xMovement = raw->data.mouse.lLastX;
        int yMovement = raw->data.mouse.lLastY;

        // Apply tolerance
        if (abs(xMovement) < TOLERANCE) xMovement = 0;
        if (abs(yMovement) < TOLERANCE) yMovement = 0;

        // Determine if TrackPoint is at rest
        bool atRest = abs(xMovement) < REST_TOLERANCE && abs(yMovement) < REST_TOLERANCE;

        if (atRest) {
            // TrackPoint is at rest, reset movement registration
            movementRegistered = false;
        } else {
            auto now = std::chrono::steady_clock::now();
            // std::cout << ((now - lastMovementTime) > MIN_INTERVAL)<< std::endl;
            if (!movementRegistered && (now - lastMovementTime) > MIN_INTERVAL) {
                // TrackPoint is not at rest and movement has not been registered yet
                if (xMovement < 0) {
                    // std::cout << "TrackPoint moved left" << std::endl;
                    SendArrowKey(VK_LEFT);
                } else if (xMovement > 0) {
                    // std::cout << "TrackPoint moved right" << std::endl;
                    SendArrowKey(VK_RIGHT);
                }

                if (yMovement < 0) {
                    // std::cout << "TrackPoint moved up" << std::endl;
                    SendArrowKey(VK_UP);
                } else if (yMovement > 0) {
                    // std::cout << "TrackPoint moved down" << std::endl;
                    SendArrowKey(VK_DOWN);
                }

                // Set movement registration and record the last movement time
                movementRegistered = true;
                lastMovementTime = std::chrono::steady_clock::now();
            }
        }
    } else {
        // Debug: Notify if non-TrackPoint device is detected
        // std::cout << "Ignoring non-TrackPoint device movement" << std::endl;
    }
}

bool RegisterTrackPoint(HWND hwnd) {
    RAWINPUTDEVICE rid[1];

    rid[0].usUsagePage = 0x01;  // Generic desktop controls
    rid[0].usUsage = 0x02;      // Mouse
    rid[0].dwFlags = RIDEV_INPUTSINK;
    rid[0].hwndTarget = hwnd;

    if (RegisterRawInputDevices(rid, 1, sizeof(rid[0])) == FALSE) {
        std::cerr << "Failed to register raw input device." << std::endl;
        return false;
    }

    return true;
}

void IdentifyTrackPoint() {
    UINT numDevices;
    GetRawInputDeviceList(nullptr, &numDevices, sizeof(RAWINPUTDEVICELIST));
    std::vector<RAWINPUTDEVICELIST> devices(numDevices);
    GetRawInputDeviceList(devices.data(), &numDevices, sizeof(RAWINPUTDEVICELIST));

    std::cout << "Listing all input devices:" << std::endl;

    std::string tempPath = "0";
    std::string tempHDevice = "0";

    for (UINT i = 0; i < numDevices; ++i) {
        char devicePath[256];
        UINT pathSize = sizeof(devicePath);
        GetRawInputDeviceInfoA(devices[i].hDevice, RIDI_DEVICENAME, devicePath, &pathSize);

        std::string deviceType;
        switch (devices[i].dwType) {
            case RIM_TYPEMOUSE:
                deviceType = "Mouse";
                break;
            case RIM_TYPEKEYBOARD:
                deviceType = "Keyboard";
                break;
            case RIM_TYPEHID:
                deviceType = "HID";
                break;
            default:
                deviceType = "Unknown";
                break;
        }

        if (!isPreConfigured) {
            std::cout << "Device " << i + 1 << ": " << devicePath << " (" << deviceType << ")" << " [" << devices[i].hDevice << "]" << std::endl;
        }
        if (isPreConfigured && trackpointDevicePath == devicePath && hDevice == hexify(devices[i].hDevice)) {
            tempPath = devicePath;
            std::cout << "TrackPoint Device Found" << std::endl;
            break;
        }
    }

    if (!isPreConfigured) {
        std::cout << std::endl;
        std::cout << "Move Trackpoint" << std::endl;
        std::cout << std::endl;
    }

    if (isPreConfigured && tempPath == "0") {
        std::cout << "TrackPoint Device Not Found" << std::endl;
        std::cout << "Delete config.ini and try again" << std::endl;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_INPUT) {
        UINT dwSize = 0;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
        LPBYTE lpb = new BYTE[dwSize];

        if (lpb == NULL) {
            return 0;
        }

        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
            std::cerr << "GetRawInputData does not return correct size!" << std::endl;
        }

        RAWINPUT* raw = (RAWINPUT*)lpb;

        if (raw->header.dwType == RIM_TYPEMOUSE) {
            char devicePath[256];
            UINT pathSize = sizeof(devicePath);
            GetRawInputDeviceInfoA(raw->header.hDevice, RIDI_DEVICENAME, devicePath, &pathSize);

            if (!isPreConfigured) {
                std::cout << "Details of Input: " << std::endl;
                // std::cout << "dwSize : " << raw->header.dwSize << std::endl;
                // std::cout << "dwType : " << raw->header.dwType << std::endl;
                std::cout << "Device Path : " << devicePath << std::endl;
                std::cout << "hDevice : " << raw->header.hDevice << std::endl;
                // std::cout << "wParam : " << raw->header.wParam << std::endl;

                std::ofstream settingsFile;
                settingsFile.open("config.ini");
                settingsFile << "devicePath=" << devicePath << std::endl;
                settingsFile << "hDevice=" << raw->header.hDevice << std::endl;
                settingsFile.close();

                trackpointDevicePath = devicePath;
                hDevice = hexify(raw->header.hDevice);

                isPreConfigured = true;
                std::cout << "Config is saved." << std::endl;
                return 0;
            } else {
                setMovement(trackpointDevicePath, raw);
            }

            // Debug: Output device path and type
        }

        delete[] lpb;
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main() {
    std::cout << "*** Thinkpad Trackpoint Remap ***" << std::endl;

    std::cout << "Loading config.ini" << std::endl;

    // Load settings from from a file which has 2 lines, devicePath = <value> and hDevice = <value>
    // This is to avoid hardcoding the devicePath and hDevice values

    // if file is not there then create it, and call Find TrackPoint function
    std::ifstream settingsFile;
    settingsFile.open("config.ini");

    if (settingsFile.is_open()) {
        std::string line;
        while (std::getline(settingsFile, line)) {
            if (line.find("devicePath") != std::string::npos) {
                trackpointDevicePath = line.substr(line.find("=") + 1);
            }
            if (line.find("hDevice") != std::string::npos) {
                hDevice = line.substr(line.find("=") + 1);
            }

            if (!trackpointDevicePath.empty() && !hDevice.empty()) {
                isPreConfigured = true;
                break;
            }
        }
        settingsFile.close();
    } else {
        std::cout << "Config is not found. Creating config.ini" << std::endl;

        std::cout << "Press Enter to Configure TrackPoint : After Pressing enter Make Sure you do not click/move anything other than TrackPoint" << std::endl;
        std::cin.get();
    }

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "TrackPointRemapClass";

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        "TrackPoint Remap",
        0,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, wc.hInstance, NULL);

    if (!hwnd) {
        std::cerr << "Failed to create window." << std::endl;
        return 1;
    }

    IdentifyTrackPoint();  // Identify the TrackPoint device

    if (!RegisterTrackPoint(hwnd)) {
        std::cerr << "Failed to register TrackPoint." << std::endl;
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
