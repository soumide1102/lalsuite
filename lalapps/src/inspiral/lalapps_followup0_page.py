import shutil
import os
import sys
import glob
import ConfigParser


# USEFUL FUNCTION DEFINITIONS

# Function to extract the followup pipeline ini file and get relevant info
# It assumes the ini file is something like *followup_pipe.ini
def getIni():
  cp = ConfigParser.ConfigParser()
  path = glob.glob('*followup_pipe.ini')
  if path[0]: cp.read(path[0])
  else:
    print "Could not find a match to *followup_pipe.ini"
    sys.exit(1)
  dest = cp.get('followup-output','page')
  server = cp.get('followup-output','url')
  return dest,server,cp

# A quick utility function to extract a list of ifos from strings
# This probably exists in other places, oh well
def ifos_from_str(ifostr):
  out = []
  for i in range(len(ifostr)/2):
    out.append(ifostr[(2*i):(2*i+2)])
  out.sort()
  return out

# using the standard pylal naming conventions for thumbs and qscan naming 
# conventions, link the image (ASSUME PATH IS TO THUMBNAIL!)
def imageLink(path):
  return '[[ImageLink('+path+','+path.replace('_thumbnail','').replace('_thumb','')+',width=400][,alt=none])]]'

# Glob the path for an image (use thumbnails)
def link_path(server, pathstr):
  path = glob.glob(pathstr)
  if path: 
    return imageLink( server+'/'+path[0] ) 
  else: return ""

def param_from_str(trig, param):
  for c in trig:
    if c.split(':')[0] == param: return c.split(':')[1]

# Turn the followup pipeline trigger output into a dictionary
# There are things true for all IFOs and IFO dependent things
def dict_from_str(trig):   
  ifos = ifos_from_str(param_from_str(trig,'Type'))
  ifodict = {}
  # ifo dependent things
  for ifo in ifos:
    paramdict = {'time':None, 'mchirp':None, 'eta':None, 'mass1':None, 'mass2':None, 'snr':None, 'chisq':None, 'chisq_dof':None,'eff_snr':None, 'duration':None}
    for param in ["time","mchirp","eta","mass1","mass2","snr","chisq","chisq_dof","eff_snr","duration"]:      
      tstr = ifo+param
      for c in trig:
        if c.split(':')[0] == tstr: paramdict[param] = c.split(':')[1]
    ifodict[ifo] = paramdict

  # things true for all ifos
  paramdict = {}
  for param in ['Rank', 'ID', 'Stat', 'FAR', 'Type', 'IfoTime']:
    paramdict[param] = param_from_str(trig, param)
  ifodict['All'] = paramdict
  return ifodict

# Put the plot snr/chisq plots in a table
def plotsnr(pdict, wiki, plot, server):
  ifos = ifos_from_str(pdict['All']['Type'])
  id = pdict['All']['ID']
  cols = []
  for p in plot:
    for ifo in ifos:
      cols.append(link_path(server, 'plotSNRCHISQJob/Images/'+ifo+'*_'+id+'_'+p+'-*_thumb.png'))
    write_columns(cols, wiki,'white','normal')
    cols = []

# Put the coherent inspiral jobs in the wiki
def plotcoh(pdict, wiki, plot, server):
  
  ifos = pdict['All']['IfoTime']
  id = pdict['All']['ID']
  cols = []
  for p in plot:
    astr = link_path(server, 'plotChiaJob/Images/'+ifos+'*_'+id+'_'+p+'-*_thumb.png')
    if astr: cols.append(astr)
    if len(cols) == 2: # just keep the table to 2 columns
      write_columns(cols, wiki,'white','normal')
      cols = []
  if cols: write_columns(cols, wiki,'white','normal')

def link_checklist(pdict, wiki, server):
  ifos = pdict['All']['Type']
  id = pdict['All']['ID']
  path = glob.glob('CHECKLIST/*-makeChecklist_*'+id+'*.html')
  if path: 
    lnstr = '[' + server +'/' + path[0] + ' Detection Checklist]'
    write_columns([lnstr],wiki,'rgb(255,200,200)', 'bold')

# put some qscan plots in the wiki in a table by ifo
def qscan(pdict, wiki, plot, server):
  ifos = ifos_from_str(pdict['All']['Type'])
  cols = []
  for p in plot:
    for ifo in ifos:
      cols.append( link_path(server, "QSCAN/foreground-hoft-qscan/"+ifo+"/"+pdict[ifo]['time']+"/"+pdict[ifo]['time']+"*1.00_"+p+"_thumbnail.png") )
    write_columns(cols, wiki,'white','normal')
    cols = []

# Color map for H1,H2,L1,V1
def get_colors():
  colordict = {'H1':'rgb(255,150,150)', 'H2':'rgb(150,150,255)', 'L1':'rgb(150,255,150)', 'V1':'rgb(255,150,255)'}
  colors = {}
  for ifo in ['H1','H2','L1','V1']:
    colors[ifo] = '<style="background-color: '+ colordict[ifo] +';">';
  return colors, colordict

# turn a list into a table row
def write_columns(tl, wiki, color='rgb(230,230,230)', style='bold'):
  for t in tl:
    wiki.write('||<style="font-weight: ' + style + '; background-color: ' + color + ';">' + t)
  wiki.write("||\n")

# put the parameters of the triggers into a table
def param_table(pdict, wiki, server):
  
  ifos = ifos_from_str(pdict['All']['Type'])
  nrows = 4+2*len(ifos)
  colors,colordict = get_colors()

  skypath = glob.glob('pylal_skyPlotJob/Images/*plot_inspiral_skymap_*' + pdict['All']['ID'] + '*_skylocation-unspecified-gpstime_thumb.png')
  if skypath:
    wiki.write('||<|'+str(nrows)+'>'+imageLink(server+'/'+skypath[0]))
  else:
    wiki.write('||<|'+str(nrows)+'>')
  write_columns(['RANK','COINC TYPE','IFO TIME','EVENT ID','STAT','FAR'],wiki)
  write_columns([pdict['All']['Rank'],pdict['All']['Type'],pdict['All']['IfoTime'],pdict['All']['ID'],pdict['All']['Stat'],pdict['All']['FAR']], wiki,'white','normal')

  # IFO DEPENDENT PARAMETERS
  write_columns(['SNR','CHISQ','CHISQ DOF','EFF SNR','DURATION'],wiki)
  for ifo in ifos:
    write_columns([ pdict[ifo]['snr'],pdict[ifo]['chisq'],pdict[ifo]['chisq_dof'],pdict[ifo]['eff_snr'],pdict[ifo]['duration'] ],wiki, colordict[ifo],'normal')
  write_columns(['TIME','MCHIRP','ETA','MASS1','MASS2'],wiki)
  for ifo in ifos:
    write_columns([pdict[ifo]['time'],pdict[ifo]['mchirp'],pdict[ifo]['eta'],pdict[ifo]['mass1'],pdict[ifo]['mass2']], wiki,colordict[ifo],'normal')

# Copy the directories into the html directory
def setup(dest, server):
  sections = ['FrCheckJob','IFOstatus_checkJob', 'QSCAN', 'analyseQscanJob', 'h1h2QeventJob', 'plotSNRCHISQJob', 'pylal_skyPlotJob', 'plotChiaJob', 'CHECKLIST','followUpTriggers']

  # Make a destination directory if it doesn't exist
  if not os.path.isdir(dest):
    print "destination:" + dest + " does not exist, I'll make it for you..."
    os.makedirs(dest)

  # copy the data to the webspace - sort of painful probably could omit some
  for sec in sections:
    print "...copying " + sec + " to " + dest
    try: shutil.copytree(sec, os.path.join(dest,sec))
    except: print "could not copy, destination probably exists"

# table of contents
def write_header(wiki):
  wiki.write('\n[[TableOfContents]]\n\n')

# how to run the followup pipeline and run this page
def how_to(log, cp, wiki):
  fulog = open('followup_pipe.log').readlines()
  wiki.write('\n\n== How to make this page ==\n\n')
  wiki.write('The followup pipeline was run with:')
  wiki.write('{{{\n')
  for l in fulog:
    wiki.write(l)
  wiki.write('}}}\n')
  wiki.write('The relevant portion of the followup ini file is:\n{{{\n[followup-triggers]\n')
  for l in cp.options('followup-triggers'):
    wiki.write(l + ' = ' + cp.get('followup-triggers',l) + '\n')
  wiki.write('}}}\n')
  wiki.write('The followup page was made by doing:')
  wiki.write('{{{\nlalapps_followup_page\n}}}\n')
  wiki.write('This gives a file called wiki.txt that you can cut and paste into your favorite moin moin spot')

def header(wiki, paramdict, text, level= 3):
  wiki.write("\n" + "="*level + " " + text + " " + paramdict['All']['ID'] + " " + "="*level + "\n")
###############################################################################
# MAIN "PROGRAM"
###############################################################################

dest, server, cp = getIni()
setup(dest, server)
triginfo = open('trigger_info.txt','r').readlines()
wiki = open('wiki.txt','w')

write_header(wiki)

# go event by event and make an entry
for line in triginfo:
  trig = line.split(',')
  paramdict = dict_from_str(trig)

  header(wiki, paramdict, "Ranked: " + paramdict['All']['Rank']+" - Follow up of event",2)
  link_checklist(paramdict, wiki, server)

  header(wiki, paramdict, "Parameters and Sky Map for:")
  param_table(paramdict, wiki, server)

  header(wiki, paramdict, "Coherent code output for:")
  plotcoh(paramdict, wiki, ["snr-squared","cohsnrh1h2nullZoomed", "cohnullsnrh1Zoomed", "phasediffh1h2Zoomed"], server)

  header(wiki, paramdict, "h(t), snr, chisq, etc for:")
  qscan(paramdict, wiki, ['timeseries_whitened','spectrogram_whitened'],server)
  plotsnr(paramdict, wiki, ['snr','rsq', 'chisq', 'template', 'white_template','fft_of_template_and_asd'],server)

# tell everyone how we did it
how_to('followup_pipe.log', cp, wiki)

# we're done
wiki.close()
