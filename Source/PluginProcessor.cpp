/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
EqTrainingAudioProcessor::EqTrainingAudioProcessor()
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
}

EqTrainingAudioProcessor::~EqTrainingAudioProcessor()
{
}

//==============================================================================
const juce::String EqTrainingAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EqTrainingAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EqTrainingAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EqTrainingAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EqTrainingAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EqTrainingAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int EqTrainingAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EqTrainingAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EqTrainingAudioProcessor::getProgramName (int index)
{
    return {};
}

void EqTrainingAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void EqTrainingAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;
    
    leftChain.prepare(spec);
    rightChain.prepare(spec);
    
    configureFilter();
}

void EqTrainingAudioProcessor::releaseResources()
{
    
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EqTrainingAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
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

void EqTrainingAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();


    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    configureFilter();
    
    juce::dsp::AudioBlock<float> block = buffer;
    
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    
    juce::dsp::ProcessContextReplacing<float> leftContext = leftBlock;
    juce::dsp::ProcessContextReplacing<float> rightContext = rightBlock;
    
    leftChain.process(leftContext);
    rightChain.process(rightContext);


}

//==============================================================================
bool EqTrainingAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* EqTrainingAudioProcessor::createEditor()
{
//    return new EqTrainingAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void EqTrainingAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts->copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
    
//    juce::MemoryOutputStream mos(destData, true);
//    apvts->state.writeToStream(mos);
}

void EqTrainingAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts->state.getType()))
            apvts->replaceState (juce::ValueTree::fromXml (*xmlState));
    configureFilter();
    
//    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
//    if( tree.isValid() )
//    {
//        apvts->replaceState(tree);
//        configureFilter();
//        
//    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EqTrainingAudioProcessor();
}
/**
 Connect
 */
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;
    
    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());

    settings.lowCutBypassed = apvts.getRawParameterValue("LowCut Bypassed")->load() > 0.5f;
    settings.peakBypassed = apvts.getRawParameterValue("Peak Bypassed")->load() > 0.5f;
    settings.highCutBypassed = apvts.getRawParameterValue("HighCut Bypassed")->load() > 0.5f;
    
    return settings;
}

/**
 Create ParameterLayout for the apvts variable constructor
 */
juce::AudioProcessorValueTreeState::ParameterLayout EqTrainingAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq",
                                                           "LowCut Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
                                                           20.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq",
                                                           "HighCut Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
                                                           20000.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq",
                                                           "Peak Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
                                                           750.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain",
                                                           "Peak Gain",
                                                           juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f),
                                                           0.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Quality",
                                                           "Peak Quality",
                                                           juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f),
                                                           1.f));
    
    juce::StringArray stringArray;
    for( int i = 0; i < 4; ++i )
    {
        juce::String str;
        str << (12 + i*12);
        str << " db/Oct";
        stringArray.add(str);
    }
    
    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope", stringArray, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", stringArray, 0));
    
    layout.add(std::make_unique<juce::AudioParameterBool>("LowCut Bypassed", "LowCut Bypassed", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Peak Bypassed", "Peak Bypassed", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("HighCut Bypassed", "HighCut Bypassed", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Analyzer Enabled", "Analyzer Enabled", true));
    
    return layout;
}

void EqTrainingAudioProcessor::configureFilter()
{
    auto chainSettings  = getChainSettings(*apvts);
    
    // ***configure the peak filter***
    auto peakCoefficients =
    juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(),
                                                        chainSettings.peakFreq,
                                                        chainSettings.peakQuality,
                                                        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
    
    *leftChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
    *rightChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;

    auto lowCutCoefficients =
    juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
                                                                                getSampleRate(),
                                                                                2 * (chainSettings.lowCutSlope + 1));
    auto highCutCoefficients =
    juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
                                                                                getSampleRate(),
                                                                                2 * (chainSettings.highCutSlope + 1));
    // ** Lowcut **
    
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    leftLowCut.setBypassed<0>(true);
    leftLowCut.setBypassed<1>(true);
    leftLowCut.setBypassed<2>(true);
    leftLowCut.setBypassed<3>(true);

    switch (chainSettings.lowCutSlope) {
        
        case Slope_12:
            *leftLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCut.setBypassed<0>(false);
        break;
        case Slope_24:
            *leftLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCut.setBypassed<0>(false);
            *leftLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            leftLowCut.setBypassed<1>(false);
        break;
        case Slope_36:
            *leftLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCut.setBypassed<0>(false);
            *leftLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            leftLowCut.setBypassed<1>(false);
            *leftLowCut.get<2>().coefficients = *lowCutCoefficients[2];
            leftLowCut.setBypassed<2>(false);
        break;
        case Slope_48:
            *leftLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCut.setBypassed<0>(false);
            *leftLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            leftLowCut.setBypassed<1>(false);
            *leftLowCut.get<2>().coefficients = *lowCutCoefficients[2];
            leftLowCut.setBypassed<2>(false);
            *leftLowCut.get<3>().coefficients = *lowCutCoefficients[3];
            leftLowCut.setBypassed<3>(false);
        break;
        }
    
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    rightLowCut.setBypassed<0>(true);
    rightLowCut.setBypassed<1>(true);
    rightLowCut.setBypassed<2>(true);
    rightLowCut.setBypassed<3>(true);

    switch (chainSettings.lowCutSlope) {
        
        case Slope_12:
            *rightLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCut.setBypassed<0>(false);
        break;
        case Slope_24:
            *rightLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCut.setBypassed<0>(false);
            *rightLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            rightLowCut.setBypassed<1>(false);
        break;
        case Slope_36:
            *rightLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCut.setBypassed<0>(false);
            *rightLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            rightLowCut.setBypassed<1>(false);
            *rightLowCut.get<2>().coefficients = *lowCutCoefficients[2];
            rightLowCut.setBypassed<2>(false);
        break;
        case Slope_48:
            *rightLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCut.setBypassed<0>(false);
            *rightLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            rightLowCut.setBypassed<1>(false);
            *rightLowCut.get<2>().coefficients = *lowCutCoefficients[2];
            rightLowCut.setBypassed<2>(false);
            *rightLowCut.get<3>().coefficients = *lowCutCoefficients[3];
            rightLowCut.setBypassed<3>(false);
        break;
        }
    
    // ** Highcut **
    
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    leftHighCut.setBypassed<0>(true);
    leftHighCut.setBypassed<1>(true);
    leftHighCut.setBypassed<2>(true);
    leftHighCut.setBypassed<3>(true);

    switch (chainSettings.highCutSlope) {
        
        case Slope_12:
            *leftHighCut.get<0>().coefficients = *highCutCoefficients[0];
            leftHighCut.setBypassed<0>(false);
        break;
        case Slope_24:
            *leftHighCut.get<0>().coefficients = *highCutCoefficients[0];
            leftHighCut.setBypassed<0>(false);
            *leftHighCut.get<1>().coefficients = *highCutCoefficients[1];
            leftHighCut.setBypassed<1>(false);
        break;
        case Slope_36:
            *leftHighCut.get<0>().coefficients = *highCutCoefficients[0];
            leftHighCut.setBypassed<0>(false);
            *leftHighCut.get<1>().coefficients = *highCutCoefficients[1];
            leftHighCut.setBypassed<1>(false);
            *leftHighCut.get<2>().coefficients = *highCutCoefficients[2];
            leftHighCut.setBypassed<2>(false);
        break;
        case Slope_48:
            *leftHighCut.get<0>().coefficients = *highCutCoefficients[0];
            leftHighCut.setBypassed<0>(false);
            *leftHighCut.get<1>().coefficients = *highCutCoefficients[1];
            leftHighCut.setBypassed<1>(false);
            *leftHighCut.get<2>().coefficients = *highCutCoefficients[2];
            leftHighCut.setBypassed<2>(false);
            *leftHighCut.get<3>().coefficients = *highCutCoefficients[3];
            leftHighCut.setBypassed<3>(false);
        break;
        }
    
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    rightHighCut.setBypassed<0>(true);
    rightHighCut.setBypassed<1>(true);
    rightHighCut.setBypassed<2>(true);
    rightHighCut.setBypassed<3>(true);

    switch (chainSettings.highCutSlope) {
        
        case Slope_12:
            *rightHighCut.get<0>().coefficients = *highCutCoefficients[0];
            rightHighCut.setBypassed<0>(false);
        break;
        case Slope_24:
            *rightHighCut.get<0>().coefficients = *highCutCoefficients[0];
            rightHighCut.setBypassed<0>(false);
            *rightHighCut.get<1>().coefficients = *highCutCoefficients[1];
            rightHighCut.setBypassed<1>(false);
        break;
        case Slope_36:
            *rightHighCut.get<0>().coefficients = *highCutCoefficients[0];
            rightHighCut.setBypassed<0>(false);
            *rightHighCut.get<1>().coefficients = *highCutCoefficients[1];
            rightHighCut.setBypassed<1>(false);
            *rightHighCut.get<2>().coefficients = *highCutCoefficients[2];
            rightHighCut.setBypassed<2>(false);
        break;
        case Slope_48:
            *rightHighCut.get<0>().coefficients = *highCutCoefficients[0];
            rightHighCut.setBypassed<0>(false);
            *rightHighCut.get<1>().coefficients = *highCutCoefficients[1];
            rightHighCut.setBypassed<1>(false);
            *rightHighCut.get<2>().coefficients = *highCutCoefficients[2];
            rightHighCut.setBypassed<2>(false);
            *rightHighCut.get<3>().coefficients = *highCutCoefficients[3];
            rightHighCut.setBypassed<3>(false);
        break;
        }
    
}

juce::AudioProcessorValueTreeState* EqTrainingAudioProcessor::getApvts(){return apvts;}
