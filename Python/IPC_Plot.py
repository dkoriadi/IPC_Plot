"""
IPC_Plot.py

Summary
-------
Entry point of the Python tool.

Retrieves data from the named pipe for plotting and then saves figure as an 
image file before updating C/C++ application success status.

NOTE: This script should be executed from the C/C++ library instead of 
running it directly because of the named pipe feature

"""

# Standard libraries
import os
from datetime import datetime

# Third-party library imports
import matplotlib.pyplot as plt
import numpy as np

import IPC_Plot_Pipe as pipe

def _processData(nColSize, nRowSize, lstData):
    """
    Split data into columns for plotting. All columns must have an equal
    number of rows (data points).

    Parameters
    ---------- 
    nColSize : int
        Number of columns in the data buffer. Corresponds to number of
        lines plotted on the figure
    
    nRowSize : int
        Number of rows of data for each column.

    lstData : list
        The data buffer containing doubles only

    Returns
    -------
    aaData : numpy 2D array
        A Numpy 2D array of the data buffer, stored column-wise. 

    """

    # Create the empty numpy 2D array
    aaData = np.empty((nRowSize, 0), dtype=np.double)
    lstTempList = []
    for i in range(nColSize):
        # Slice the list by column and append to numpy 2D array
        lstTempList = lstData[ i * nRowSize : ( i + 1 ) * nRowSize]
        aColData = np.array(lstTempList)[:, None]
        aaData = np.concatenate((aaData, aColData), axis=1)
    return aaData

def _saveImage(aaData, lstGraphLabels):
    """
    Creates an image of the graph figure from the numpy 2D array and 
    saves image file in the same directory as this script. 
    Default image format is *.png

    Parameters
    ---------- 
    aaData : numpy 2D array
        A Numpy 2D array of the data buffer, stored column-wise. 

    lstGraphLabels : list
        A list of user labels to be displayed on the legend of the 
        figure
        
    """
    szImageName = _plot(aaData, lstGraphLabels)
    # Increase dpi for higher resolution images, default dpi is 100     
    plt.savefig(szImageName, dpi=200)

def _plot(aaData, lstGraphLabels):
    """
    Creates the Matplotlib objects such as the Figure, Axes and lines
    for all columns in the data buffer
    
    Parameters
    ---------- 
    aaData : numpy 2D array
        A Numpy 2D array of the data buffer, stored column-wise. 

    lstGraphLabels : list
        A list of user labels to be displayed on the legend of the 
        figure

    Returns
    -------
    szTitle : string
        Title of the figure, used for naming the image file to be 
        saved   

    """

    fig, ax = plt.subplots()
    plt.subplots_adjust(left=0.10, right=0.975)

    # Auto-generate x-axis based on the number of rows in dataset
    lstXValues = np.arange(0, aaData.shape[0]).tolist()

    # Iterate all columns in dataset
    for i in range(aaData.shape[1]):
        # NumPy syntax: dataset[<start row>,<start col>:<end row>,<end col>]
        lstColPlot = aaData[:, i]
        ax.plot(lstXValues,
                lstColPlot,
                marker="."  # Display each data point as a dot
        ) 

    # Put a legend at the best position in the figure (determined by Matplotlib)
    # Default font size is 10
    ax.legend(loc='best', labels=lstGraphLabels, prop={"size":8})

    # Set x-axis label
    ax.set_xlabel("Samples")
    # Set graph title based on current date and time
    szTitle = datetime.now().strftime("IMG_%Y%m%d_%HH%MM%SS")
    ax.set_title(szTitle)

    # Enable grid on figure
    ax.minorticks_on()
    ax.grid(which='major', color='#CCCCCC', linestyle='--')
    ax.grid(which='minor', color='#CCCCCC', linestyle=':')
    return szTitle

def main():
    """
    Retrieves data from the named pipe for plotting and then saves figure 
    as an image before updating C/C++ application success status.

    """
    nColSize, nRowSize, lstData, lstGraphLabels = pipe.retrieveData()
    aaData = _processData(nColSize, nRowSize, lstData) 
    _saveImage(aaData, lstGraphLabels)
    pipe.updateStatus()

if __name__ == '__main__':
    """ Entry point """
    main()