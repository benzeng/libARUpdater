/*
    Copyright (C) 2014 Parrot SA

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the 
      distribution.
    * Neither the name of Parrot nor the names
      of its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
    OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
    AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
    OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
*/

package com.parrot.arsdk.arupdater;

import java.lang.Runnable;
import com.parrot.arsdk.arsal.ARSALPrint;
import com.parrot.arsdk.ardiscovery.ARDISCOVERY_PRODUCT_ENUM;
import com.parrot.arsdk.arutils.ARUtilsManager;
import com.parrot.arsdk.arsal.ARSALMd5Manager;


public class ARUpdaterUploader
{

	private static final String TAG = "ARUpdaterUploader";

	/* Native Functions */
	private static native void nativeStaticInit();
    private native int nativeNew(long manager, String rootFolder, long utilsManager, long md5Manager, int discoveryProduct, 
    	ARUpdaterPlfUploadProgressListener plfUploadProgressListener, Object progressArgs, 
    	ARUpdaterPlfUploadCompletionListener plfUploadCompletionListener, Object completionArgs);
    private native int nativeDelete(long manager);
    private native void nativeThreadRun (long manager);
    private native int nativeCancelThread (long manager);

    private long nativeManager = 0;
    private Runnable uploaderRunnable = null;
    private boolean isInit = false;

    /*  Static Block */
    static
    {
        nativeStaticInit();
    }  

    protected ARUpdaterUploader(long _nativeManager)
    {
    	this.nativeManager = _nativeManager;
    	this.uploaderRunnable = new Runnable() {
    		public void run() {
    			nativeThreadRun(nativeManager);
    		}
    	};
    }


	/**
     * Creates the ARUpdater Uploader
     * @param rootFolder The root folder
     * @param utilsManager The utils manager initialized with the correct network (wifi or ble)
     * @param product see {@link ARDISCOVERY_PRODUCT_ENUM}
     * @param plfUploadProgressListener The available progress listener
     * @param progressArgs The available progress listener arg
     * @param completionArgs The available completion listener
     * @param progressArgs The available completion listener arg
     * @return void
     * @throws ARUpdaterException if error
     */
    public void createUpdaterUploader(String rootFolder, ARUtilsManager utilsManager, ARSALMd5Manager md5Manager, ARDISCOVERY_PRODUCT_ENUM product, 
        ARUpdaterPlfUploadProgressListener plfUploadProgressListener, Object progressArgs, 
        ARUpdaterPlfUploadCompletionListener plfUploadCompletionListener, Object completionArgs) throws ARUpdaterException
    {
    	int result = nativeNew(nativeManager, rootFolder, utilsManager.getManager(), md5Manager.getNativeManager(), product.getValue(), plfUploadProgressListener, progressArgs, plfUploadCompletionListener, completionArgs);

    	ARUPDATER_ERROR_ENUM error = ARUPDATER_ERROR_ENUM.getFromValue(result);

    	if (error != ARUPDATER_ERROR_ENUM.ARUPDATER_OK)
    	{
    		throw new ARUpdaterException();
    	}
    	else
    	{
    		this.isInit = true;
    	}
    }

	/**
     * Deletes the ARUpdater Uploader
     * @return ARUPDATER_OK if success, else an {@link ARUPDATER_ERROR_ENUM} error code
     */
    public ARUPDATER_ERROR_ENUM dispose()
    {
        ARUPDATER_ERROR_ENUM error = ARUPDATER_ERROR_ENUM.ARUPDATER_OK;
        
        if (isInit)
        {
            int result = nativeDelete(nativeManager);
            
            error = ARUPDATER_ERROR_ENUM.getFromValue(result);
            if (error == ARUPDATER_ERROR_ENUM.ARUPDATER_OK)
            {
                isInit = false;
            }
        }
        
        return error;
    }

    /**
     * Destructor<br>
     * This destructor tries to avoid leaks if the object was not disposed
     */
    protected void finalize () throws Throwable
    {
        try
        {
            if (isInit)
            {
                ARSALPrint.e (TAG, "Object " + this + " was not disposed !");
                ARUPDATER_ERROR_ENUM error = dispose ();
                if(error != ARUPDATER_ERROR_ENUM.ARUPDATER_OK)
                {
                    ARSALPrint.e (TAG, "Unable to dispose object " + this + " ... leaking memory !");
                }
            }
        }
        finally
        {
            super.finalize ();
        }
    }

    public ARUPDATER_ERROR_ENUM cancel()
    {
    	int result = nativeCancelThread(nativeManager);

    	ARUPDATER_ERROR_ENUM error = ARUPDATER_ERROR_ENUM.getFromValue(result);

    	return error;
    }


    public Runnable getUploaderRunnable()
    {
        Runnable runnable = null;

        if (isInit == true)
        {
            runnable = this.uploaderRunnable;
        }

        return runnable;
    }


}
