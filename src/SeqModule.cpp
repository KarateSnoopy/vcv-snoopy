#include "SeqModule.h"

SEQ::SEQ() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS)
{
}

json_t *SEQ::toJson()
{
    json_t *rootJ = json_object();

    // running
    json_object_set_new(rootJ, "running", json_boolean(running));

    // gates
    json_t *gatesJ = json_array();
    for (int i = 0; i < MAX_STEPS; i++)
    {
        json_t *gateJ = json_integer((int)gateState[i]);
        json_array_append_new(gatesJ, gateJ);
    }
    json_object_set_new(rootJ, "gates", gatesJ);

    // gateMode
    json_t *gateModeJ = json_integer((int)gateMode);
    json_object_set_new(rootJ, "gateMode", gateModeJ);

    return rootJ;
}

void SEQ::fromJson(json_t *rootJ)
{
    // running
    json_t *runningJ = json_object_get(rootJ, "running");
    if (runningJ)
        running = json_is_true(runningJ);

    // gates
    json_t *gatesJ = json_object_get(rootJ, "gates");
    if (gatesJ)
    {
        for (int i = 0; i < MAX_STEPS; i++)
        {
            json_t *gateJ = json_array_get(gatesJ, i);
            if (gateJ)
                gateState[i] = !!json_integer_value(gateJ);
        }
    }

    // gateMode
    json_t *gateModeJ = json_object_get(rootJ, "gateMode");
    if (gateModeJ)
        gateMode = (GateMode)json_integer_value(gateModeJ);
}

void SEQ::initialize()
{
    for (int i = 0; i < MAX_STEPS; i++)
    {
        gateState[i] = false;
    }
}

void SEQ::randomize()
{
    for (int i = 0; i < MAX_STEPS; i++)
    {
        gateState[i] = (randomf() > 0.5);
    }
}

void SEQ::step()
{
    const float lightLambda = 0.075;
    _frameCount++;
    bool nextStep = false;

    // Run
    if (m_runningButton.Process(params))
    {
        running = !running;
    }

    if (running)
    {
        if (inputs[EXT_CLOCK_INPUT].active)
        {
            // External clock
            if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].value))
            {
                phase = 0.0;
                nextStep = true;
            }
        }
        else
        {
            // Internal clock
            float clockTime = powf(2.0, params[CLOCK_PARAM].value + inputs[CLOCK_INPUT].value);
            phase += clockTime / gSampleRate;
            if (phase >= 1.0)
            {
                phase -= 1.0;
                nextStep = true;
            }
        }
    }

    if (m_resetButton.ProcessWithInput(params, inputs))
    {
        phase = 0.0;
        index = MAX_STEPS;
        nextStep = true;
    }

    if (m_pitchEditButton.Process(params))
    {
        m_gateEditButton.SetOnOff(true, false);
        m_pitchEditButton.SetOnOff(true, true);

        for (auto &param : m_editPitchUI)
        {
            param->visible = true;
        }
    }

    if (m_gateEditButton.Process(params))
    {
        m_gateEditButton.SetOnOff(true, true);
        m_pitchEditButton.SetOnOff(true, false);

        for (auto &param : m_editPitchUI)
        {
            param->visible = false;
        }
    }

    if (nextStep)
    {
        // Advance step
        int numSteps = clampi(roundf(params[STEPS_PARAM].value + inputs[STEPS_INPUT].value), 1, MAX_STEPS);
        index += 1;
        if (index >= numSteps)
        {
            index = 0;
        }
        stepLights[index] = 1.0;
        gatePulse.trigger(1e-3);
    }

    bool pulse = gatePulse.process(1.0 / gSampleRate);

    // Gate buttons
    for (int i = 0; i < MAX_STEPS; i++)
    {
        // if (gateTriggers[i].process(params[GATE_PARAM + i].value))
        // {
        //     //write_log(0, "v %d %f\n", i, params[GATE_PARAM + i].value);
        //     gateState[i] = !gateState[i];
        // }

        bool gateOn = (running && i == index && gateState[i]);
        if (gateMode == TRIGGER)
            gateOn = gateOn && pulse;
        else if (gateMode == RETRIGGER)
            gateOn = gateOn && !pulse;

        outputs[GATE_X_OUTPUT].value = gateOn ? 10.0 : 0.0; // TODO
        stepLights[i] -= stepLights[i] / lightLambda / gSampleRate;
        //gateLights[i] = gateState[i] ? 1.0 - stepLights[i] : stepLights[i];
        gateLights[i] = stepLights[i];

        //write_log(40000, "gateLights[%d]=%f\n", i, gateLights[i]);
    }

    //write_log(40000, "g trigger %d %d %d %d\n", gateState[0], gateState[1], gateState[2], gateState[3]);

    // Rows
    float row1 = params[ROW1_PARAM + index].value;
    //float row2 = 0.0f; //params[ROW2_PARAM + index].value;
    //float row3 = 0.0f; //params[ROW3_PARAM + index].value;
    bool gatesOn = (running && gateState[index]);
    if (gateMode == TRIGGER)
        gatesOn = gatesOn && pulse;
    else if (gateMode == RETRIGGER)
        gatesOn = gatesOn && !pulse;

    // Outputs
    outputs[CV_OUTPUT].value = row1;
    //outputs[ROW2_OUTPUT].value = row2;
    //outputs[ROW3_OUTPUT].value = row3;
    outputs[GATE_X_OUTPUT].value = gatesOn ? 10.0 : 0.0;
    gateXLight = gatesOn ? 1.0 : 0.0;
    //rowLights[0] = row1;
    //rowLights[1] = row2;
    //rowLights[2] = row3;
}

void SEQ::InitUI(ModuleWidget *moduleWidget, Rect box)
{
    m_moduleWidget = moduleWidget;
    Module *module = this;

    {
        SVGPanel *panel = new SVGPanel();
        panel->box.size = box.size;
        panel->setBackground(SVG::load(assetPlugin(plugin, "res/SeqModule.svg")));
        addChild(panel);
    }

    addParam(createParam<Davies1900hSmallBlackKnob>(Vec(18, 56), module, SEQ::CLOCK_PARAM, -2.0, 6.0, 2.0));
    addParam(createParam<Davies1900hSmallBlackSnapKnob>(Vec(132, 56), module, SEQ::STEPS_PARAM, 1.0, MAX_STEPS, MAX_STEPS));
    addChild(createValueLight<SmallLight<GreenValueLight>>(Vec(180, 65), &cvLight));
    addChild(createValueLight<SmallLight<GreenValueLight>>(Vec(219, 65), &gateXLight));
    addChild(createValueLight<SmallLight<GreenValueLight>>(Vec(257, 65), &gateYLight));
    //addChild(createValueLight<SmallLight<GreenValueLight>>(Vec(296, 65), &rowLights[2]));

    m_runningButton.Init(m_moduleWidget, module, 60, 60, SEQ::RUN_PARAM);
    m_runningButton.SetOnOff(true, true);
    m_resetButton.Init(m_moduleWidget, module, 99, 60, SEQ::RESET_PARAM);
    m_resetButton.AddInput(SEQ::RESET_INPUT);

    int editButtonX = 50;
    int editButtonY = 150;

    m_pitchEditButton.Init(m_moduleWidget, module, editButtonX, editButtonY, SEQ::PITCH_EDIT_PARAM);
    m_pitchEditButton.SetOnOff(true, true);

    editButtonY += 20;
    m_gateEditButton.Init(m_moduleWidget, module, editButtonX, editButtonY, SEQ::GATE_EDIT_PARAM);
    m_gateEditButton.SetOnOff(true, false);

    static const float portX[8] = {20, 58, 96, 135, 173, 212, 250, 289};
    addInput(createInput<PJ301MPort>(Vec(portX[0] - 1, 98), module, SEQ::CLOCK_INPUT));
    addInput(createInput<PJ301MPort>(Vec(portX[1] - 1, 98), module, SEQ::EXT_CLOCK_INPUT));
    addInput(createInput<PJ301MPort>(Vec(portX[2] - 1, 98), module, SEQ::RESET_INPUT));
    addInput(createInput<PJ301MPort>(Vec(portX[3] - 1, 98), module, SEQ::STEPS_INPUT));
    addOutput(createOutput<PJ301MPort>(Vec(portX[4] - 1, 98), module, SEQ::CV_OUTPUT));
    addOutput(createOutput<PJ301MPort>(Vec(portX[5] - 1, 98), module, SEQ::GATE_X_OUTPUT));
    addOutput(createOutput<PJ301MPort>(Vec(portX[6] - 1, 98), module, SEQ::GATE_Y_OUTPUT));
    //addOutput(createOutput<PJ301MPort>(Vec(portX[7] - 1, 98), module, SEQ::ROW3_OUTPUT));

    static const float btn_x[4] = {0, 38, 76, 115};
    static const float btn_y[4] = {0, 38, 76, 115};
    int iZ = 0;
    for (int iY = 0; iY < 4; iY++)
    {
        for (int iX = 0; iX < 4; iX++)
        {
            int x = btn_x[iX] + 100;
            int y = btn_y[iY] + 177;

            m_editPitchUI.push_back(addParam(createParam<RoundBlackKnob>(Vec(x, y), module, SEQ::ROW1_PARAM + iZ, 0.0, 6.0, 0.0)));
            m_editPitchUI.push_back(addChild(createValueLight<SmallLight<GreenValueLight>>(Vec(x + 15, y + 15), &gateLights[iZ])));
            iZ++;
        }
    }
}

Widget *SEQ::addChild(Widget *widget)
{
    m_moduleWidget->addChild(widget);
    return widget;
}

ParamWidget *SEQ::addParam(ParamWidget *param)
{
    m_moduleWidget->addParam(param);
    return param;
}

Port *SEQ::addInput(Port *input)
{
    m_moduleWidget->addInput(input);
    return input;
}

Port *SEQ::addOutput(Port *output)
{
    m_moduleWidget->addOutput(output);
    return output;
}
