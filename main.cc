#pragma once

#define _WINDOWS_LEAN_AND_MEAN
#define _USE_MATH_DEFINES

#include <Windows.h>
#include <TlHelp32.h>

#include <chrono>
#include <thread>
#include <random>
#include <string>
#include <vector>

#include <iostream>

// minecraft used as an example
HWND TARGET = FindWindowA("LWJGL", 0);

namespace Clicker
{

    // example config
    namespace Customization
    {
        bool bToggled{ true };
        bool bDrop{ true };
        bool bSpike{ true };

        float flMinimumHoldLength{ 20 };
        float flMaximumHoldLength{ 45 };
        
        // bounds for shifting variance
        float flMinimumShift{ -0.1 };
        float flMaximumShift{ 0.1 };

        // bounds for variance
        float flMinimumVariance{ 0.5 };
        float flMaximumVariance{ 1.5 };

        int iDropProbability{ 15 };
        int iSpikeProbability{ 15 };

        int iClicksPerSecond{ 12 };

        // count of clicks before variance is shifted
        int iRefreshThreshold{ 50 };
    }

    namespace Constants
    {
        double dbRefreshRate{ 0 };

        inline double GetRefreshRate()
        {
            DEVMODE lpDevMode;
            memset(&lpDevMode, 0, sizeof(DEVMODE));
            lpDevMode.dmSize = sizeof(DEVMODE);
            lpDevMode.dmDriverExtra = 0;
            EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &lpDevMode);
            return lpDevMode.dmDisplayFrequency;
        }
    }

    namespace Randomization
    {
        std::random_device dev;
        
        inline float RNG(float min, float max)
        {
            std::mt19937_64 rng(dev());
            std::uniform_real_distribution<> dist(min, max);
            return dist(rng);
        }
    }

   
    namespace Input
    {
        POINT ptMousePosition{ 0.f, 0.f };

        int iTotalClicks{ 0 };
        int iResetClicks{ 0 };

        // left click func
        inline void Click(int hold, HWND target)
        {
            // post click to current window operation queue
            PostMessageW(target, WM_LBUTTONDOWN, 1, MAKELPARAM(ptMousePosition.x, ptMousePosition.y));
            std::this_thread::sleep_for(std::chrono::microseconds(hold));
            PostMessageW(target, WM_LBUTTONUP, 0, MAKELPARAM(ptMousePosition.x, ptMousePosition.y));

            // keep track of clicks sent so far
            iTotalClicks++;
            iResetClicks++;
        }

        // stupid cursor tracking
        extern void Track()
        {
            // convert monitor refresh rate to microseconds to hold precision
            int iRefresh = Constants::dbRefreshRate * 1000;

            while (1)
            {
                GetCursorPos(&ptMousePosition);
                std::this_thread::sleep_for(std::chrono::microseconds(iRefresh));
            }
        }
    }

    // internal data
    namespace Data
    {
        int iThreadSleep{ 0 };

        float flClicksPerSecond{ 0 };
        float flHoldLength{ 0 };

        float flDropChance{ 0 };
        float flSpikeChance{ 0 };

        float flVariance{ 1.f };

        int iRunTime{ 0 };

        // run time tracking func
        extern void Track()
        {
            while (1)
            {
                iRunTime++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
    }

    extern void Run()
    {
        // grab the monitor refresh rate
        Constants::dbRefreshRate = Constants::GetRefreshRate();

        // thread to track cursor
        std::thread thInput(Input::Track);
        thInput.detach();

        // thread to track run time (yes i know its not accurate if its not first (((cope))))
        std::thread thData(Data::Track);
        thData.detach();

        while (1)
        {
            // when to click (turned on and left mouse is held)
            if (Customization::bToggled && GetAsyncKeyState(VK_LBUTTON))
            {
                // start operation counter
                auto atStart = std::chrono::high_resolution_clock::now();

                // refresh rng shifting based on a "reset" value
                if (Input::iResetClicks >= Customization::iRefreshThreshold)
                {
                    // bound our shift
                    if (Data::flVariance < Customization::flMinimumVariance || Data::flVariance > Customization::flMaximumVariance)
                        Data::flVariance = 1;
                    else
                        // add to the shift (possibly a negative value)
                        Data::flVariance += Randomization::RNG(Customization::flMinimumShift, Customization::flMaximumShift);

                    Input::iResetClicks = 0;
                }

                // generate a base cps value
                Data::flClicksPerSecond = Randomization::RNG(Customization::iClicksPerSecond - Data::flVariance, Customization::iClicksPerSecond + Data::flVariance);

                // generate a hold length for the click
                Data::flHoldLength = Randomization::RNG(Customization::flMinimumHoldLength, Customization::flMaximumHoldLength);

                // handle drops and spikes by manipulating thread sleep time
                if (Customization::bDrop && Customization::iDropProbability > 0)
                {
                    Data::flDropChance = Randomization::RNG(0, 100);
                    if (Data::flDropChance > 0 && Data::flDropChance <= Customization::iDropProbability)
                    {
                        // add to sleep time
                        Data::iThreadSleep += Randomization::RNG(0, 100) * 1000;
                    }
                }

                if (Customization::bSpike && Customization::iSpikeProbability > 0)
                {
                    Data::flSpikeChance = Randomization::RNG(0, 100);
                    if (Data::flSpikeChance > 0 && Data::flSpikeChance <= Customization::iSpikeProbability)
                    {
                        // subtract from sleep time
                        Data::iThreadSleep -= Randomization::RNG(0, 100) * 1000;
                    }
                }

                // queue click input to target window, keep track of sent clicks
                Input::Click(Data::flHoldLength, TARGET);
                std::cout << "Total: " << Input::iTotalClicks << " | Reset: " << Input::iResetClicks << " | Variance: " << Data::flVariance << std::endl;

                // stop operation counter
                auto atStop = std::chrono::high_resolution_clock::now();

                // operation offset in milliseconds
                auto atOffset = std::chrono::duration_cast<std::chrono::milliseconds>(atStop - atStart).count() / 1000;
         
                // microsecond value the thread should sleep for, accounting for operation offset
                Data::iThreadSleep = ((1000 / Data::flClicksPerSecond) * 1000) - (atOffset * 1000);

                // sleep the thread
                std::this_thread::sleep_for(std::chrono::microseconds(Data::iThreadSleep));
            }
            else { std::this_thread::sleep_for(std::chrono::microseconds(1)); }
        }
    }
}

int main()
{
    Clicker::Run();
   
    return 0;
}
