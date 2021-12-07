/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"



//==============================================================================
SimpleMbCompAudioProcessor::SimpleMbCompAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    using namespace Params;
    const auto& params = GetParams();
    
    auto floatHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
    {
        param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);
        
    };
    
    auto choiceHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
        {
            param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(params.at(paramName)));
            jassert(param != nullptr);
            
        };
    
    auto boolHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
       {
           param = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(params.at(paramName)));
           jassert(param != nullptr);
           
       };
    
    floatHelper(lowBandComp.attack, Names::Attack_Low_Band);
    floatHelper(lowBandComp.release, Names::Release_Low_Band);
    floatHelper(lowBandComp.threshold, Names::Threshold_Low_band);
    
    floatHelper(midBandComp.attack, Names::Attack_Mid_Band);
    floatHelper(midBandComp.release, Names::Release_Mid_Band);
    floatHelper(midBandComp.threshold, Names::Threshold_Mid_band);
    
    floatHelper(highBandComp.attack, Names::Attack_High_Band);
    floatHelper(highBandComp.release, Names::Release_High_Band);
    floatHelper(highBandComp.threshold, Names::Threshold_High_band);

    choiceHelper(lowBandComp.ratio, Names::Ratio_Low_Band);
    choiceHelper(midBandComp.ratio, Names::Ratio_Mid_Band);
    choiceHelper(highBandComp.ratio, Names::Ratio_High_Band);
    
    boolHelper(lowBandComp.bypassed, Names::Bypassed_Low_Band);
    boolHelper(midBandComp.bypassed, Names::Bypassed_Mid_Band);
    boolHelper(highBandComp.bypassed, Names::Bypassed_High_Band);
    
    boolHelper(lowBandComp.mute, Names::Mute_Low_Band);
    boolHelper(midBandComp.mute, Names::Mute_Mid_Band);
    boolHelper(highBandComp.mute, Names::Mute_High_Band);
    
    boolHelper(lowBandComp.solo, Names::Solo_Low_Band);
    boolHelper(midBandComp.solo, Names::Solo_Mid_Band);
    boolHelper(highBandComp.solo, Names::Solo_High_Band);
    
    LP1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    LP2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    
    
    AP2.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
    
    floatHelper(lowMidCrossover, Names::Low_Mid_Crossover_Freq);
    floatHelper(midHighCrossover, Names::Mid_High_Crossover_Freq);
    
    floatHelper(inputGainParam, Names::Gain_In);
    floatHelper(outputGainParam, Names::Gain_Out);
   
}

SimpleMbCompAudioProcessor::~SimpleMbCompAudioProcessor()
{
}

//==============================================================================
const juce::String SimpleMbCompAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleMbCompAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleMbCompAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleMbCompAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleMbCompAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleMbCompAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleMbCompAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleMbCompAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleMbCompAudioProcessor::getProgramName (int index)
{
    return {};
}

void SimpleMbCompAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SimpleMbCompAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();
    spec.sampleRate = sampleRate;
    
    for (auto& comp : compressors)
        comp.prepare(spec);
    
    
    LP1.prepare(spec);
    HP1.prepare(spec);
    
    AP2.prepare(spec);
    
   
    LP2.prepare(spec);
    HP2.prepare(spec);
    
    inputGain.prepare(spec);
    outputGain.prepare(spec);
    
    inputGain.setRampDurationSeconds(0.05);
    outputGain.setRampDurationSeconds(0.05);
    
    for (auto& buffer : filterBuffers)
    {
        buffer.setSize(spec.numChannels, samplesPerBlock);
    }
}

void SimpleMbCompAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleMbCompAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SimpleMbCompAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    

//    compressor.process(buffer);
    
    for (auto& compressor : compressors)
    {
        compressor.updateCompressorSettings();
    }
    
    inputGain.setGainDecibels(inputGainParam->get());
    outputGain.setGainDecibels(outputGainParam->get());
    
    applyGain(buffer, inputGain);
    
    for (auto& fb : filterBuffers)
    {
        fb = buffer;
    }
    
    
    auto lowMidCutoffFreq = lowMidCrossover->get();
    LP1.setCutoffFrequency(lowMidCutoffFreq);
    HP1.setCutoffFrequency(lowMidCutoffFreq);
    
    
    auto midHighCutoffFreq = midHighCrossover->get();
    AP2.setCutoffFrequency(midHighCutoffFreq);
    LP2.setCutoffFrequency(midHighCutoffFreq);
    HP2.setCutoffFrequency(midHighCutoffFreq);
   
    
    auto fb0Block = juce::dsp::AudioBlock<float>(filterBuffers[0]);
    auto fb1Block = juce::dsp::AudioBlock<float>(filterBuffers[1]);
    auto fb2Block = juce::dsp::AudioBlock<float>(filterBuffers[2]);
    
   
    
    auto fb0Ctx = juce::dsp::ProcessContextReplacing<float>(fb0Block);
    auto fb1Ctx = juce::dsp::ProcessContextReplacing<float>(fb1Block);
    auto fb2Ctx = juce::dsp::ProcessContextReplacing<float>(fb2Block);
    
    LP1.process(fb0Ctx);
    AP2.process(fb0Ctx);
    
    HP1.process(fb1Ctx);
    filterBuffers[2] = filterBuffers[1];
    
    LP2.process(fb1Ctx);
    HP2.process(fb2Ctx);
    
    for (size_t i = 0; i < filterBuffers.size(); i++)
    {
        compressors[i].process(filterBuffers[i]);
    }
    
    
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();
    
    

    
    buffer.clear();
    
    
    
    auto addFilterBand = [nc = numChannels, ns = numSamples](auto& inputBuffer, const auto& source)
    {
        for (auto i = 0; i < nc; i++)
        {
            inputBuffer.addFrom(i, 0, source, i, 0, ns);
        }
    };
    
    auto bandsAreSoloed = false;
    
    for(auto& comp : compressors)
    {
        if(comp.solo->get())
        {
            bandsAreSoloed = true;
            break;
        }
    }

    if  (bandsAreSoloed)
    {
        for (size_t i = 0; i < compressors.size(); i++)
        {
            auto& comp = compressors[i];
            if (comp.solo->get() )
            {
                addFilterBand(buffer, filterBuffers[i]);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < compressors.size(); i++)
        {
            auto& comp = compressors[i];
            if ( ! comp.mute->get() )
            {
                addFilterBand(buffer, filterBuffers[i]);
            }
        }
    }
    
    applyGain(buffer, outputGain);
//    addFilterBand(buffer, filterBuffers[0]);
//    addFilterBand(buffer, filterBuffers[1]);
//    addFilterBand(buffer, filterBuffers[2]);
    
}

//==============================================================================
bool SimpleMbCompAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleMbCompAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SimpleMbCompAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void SimpleMbCompAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleMbCompAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    
    using namespace juce;
    using namespace Params;
    
    
    const auto& params = GetParams();
    auto attackReleaseRange = NormalisableRange<float>(5, 500, 1, 1);
    auto gainRange = NormalisableRange<float>(-24.f, 24.f, 0.5f, 1);

    
    //********************************** COMPRESSORS PARAMETERS
    //*********************************************************GAIN/OUTPUT
    
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Gain_In),
                                                     params.at(Names::Gain_In),
                                                     gainRange,
                                                     0));
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Gain_Out),
                                                     params.at(Names::Gain_Out),
                                                     gainRange,
                                                     0));
    
    
    //*************************************************************** THRESHOLD
    
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Threshold_Low_band),
                                                     params.at(Names::Threshold_Low_band),
                                                     NormalisableRange<float>(-60, 12, 1, 1),
                                                     0));
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Threshold_Mid_band),
                                                     params.at(Names::Threshold_Mid_band),
                                                     NormalisableRange<float>(-60, 12, 1, 1),
                                                     0));
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Threshold_High_band),
                                                     params.at(Names::Threshold_High_band),
                                                     NormalisableRange<float>(-60, 12, 1, 1),
                                                     0));
          
    //**************************************************************** ATTACK
    
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Attack_Low_Band),
                                                     params.at(Names::Attack_Low_Band),
                                                     attackReleaseRange,
                                                     50));
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Attack_Mid_Band),
                                                     params.at(Names::Attack_Mid_Band),
                                                     attackReleaseRange,
                                                     50));
    
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Attack_High_Band),
                                                     params.at(Names::Attack_High_Band),
                                                     attackReleaseRange,
                                                     50));
    
    //***************************************************************** RELEASE
    
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Release_Low_Band),
                                                     params.at(Names::Release_Low_Band),
                                                     attackReleaseRange,
                                                     250));
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Release_Mid_Band),
                                                     params.at(Names::Release_Mid_Band),
                                                     attackReleaseRange,
                                                     250));
    
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Release_High_Band),
                                                     params.at(Names::Release_High_Band),
                                                     attackReleaseRange,
                                                     250));
    
    //*************************************************************************************
    
    auto choices = std::vector<double> {1, 1.5, 2, 3, 4, 5, 6, 7, 8, 10, 15, 20, 50, 100};
    juce::StringArray sa;
    for (auto choice:choices)
    {
        sa.add( String(choice, 1));
    }
    
    
    //**************************************************************** RATIO
    
    layout.add(std::make_unique<AudioParameterChoice>(params.at(Names::Ratio_Low_Band),
                                                      params.at(Names::Ratio_Low_Band),
                                                      sa,
                                                      3));
    
    layout.add(std::make_unique<AudioParameterChoice>(params.at(Names::Ratio_Mid_Band),
                                                      params.at(Names::Ratio_Mid_Band),
                                                      sa,
                                                      3));
    
    layout.add(std::make_unique<AudioParameterChoice>(params.at(Names::Ratio_High_Band),
                                                      params.at(Names::Ratio_High_Band),
                                                      sa,
                                                      3));
    
    
    //**************************************************************** BYPASS
    
    layout.add(std::make_unique<AudioParameterBool>(params.at(Names::Bypassed_Low_Band),
                                                    params.at(Names::Bypassed_Low_Band),
                                                    false));
    
    layout.add(std::make_unique<AudioParameterBool>(params.at(Names::Bypassed_Mid_Band),
                                                    params.at(Names::Bypassed_Mid_Band),
                                                    false));
    
    layout.add(std::make_unique<AudioParameterBool>(params.at(Names::Bypassed_High_Band),
                                                    params.at(Names::Bypassed_High_Band),
                                                    false));
    
    
    //**************************************************************** MUTE
    
    layout.add(std::make_unique<AudioParameterBool>(params.at(Names::Mute_Low_Band),
                                                    params.at(Names::Mute_Low_Band),
                                                    false));
    
    layout.add(std::make_unique<AudioParameterBool>(params.at(Names::Mute_Mid_Band),
                                                    params.at(Names::Mute_Mid_Band),
                                                    false));
    
    layout.add(std::make_unique<AudioParameterBool>(params.at(Names::Mute_High_Band),
                                                    params.at(Names::Mute_High_Band),
                                                    false));
    
    
    //**************************************************************** SOLO
    
    layout.add(std::make_unique<AudioParameterBool>(params.at(Names::Solo_Low_Band),
                                                    params.at(Names::Solo_Low_Band),
                                                    false));
    
    layout.add(std::make_unique<AudioParameterBool>(params.at(Names::Solo_Mid_Band),
                                                    params.at(Names::Solo_Mid_Band),
                                                    false));
    
    layout.add(std::make_unique<AudioParameterBool>(params.at(Names::Solo_High_Band),
                                                    params.at(Names::Solo_High_Band),
                                                    false));
    
    
    //**************************************************************** BANDS CROSSOVERS
    
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Low_Mid_Crossover_Freq),
                                                     params.at(Names::Low_Mid_Crossover_Freq),
                                                     NormalisableRange<float>(20, 999, 1, 1),
                                                     400));
    
    layout.add(std::make_unique<AudioParameterFloat>(params.at(Names::Mid_High_Crossover_Freq),
                                                     params.at(Names::Mid_High_Crossover_Freq),
                                                     NormalisableRange<float>(1000, 20000, 1, 1),
                                                     2000));
    
    
    
    
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleMbCompAudioProcessor();
}
