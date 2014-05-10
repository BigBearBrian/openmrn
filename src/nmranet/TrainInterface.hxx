/** \copyright
 * Copyright (c) 2014, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file TrainInterface.hxx
 *
 * Defines an interface to talk to train implementations.
 *
 * @author Balazs Racz
 * @date 10 May 2014
 */

#ifndef _NMRANET_TRAININTERFACE_HXX_
#define _NMRANET_TRAININTERFACE_HXX_

#include "nmranet/TractionDefs.hxx"

namespace NMRAnet {

class TrainImpl
{
public:
    /** Sets the speed of the locomotive.
     * @param speed is the requested scale speed in m/s. The sign of the number
     * means the direction.
     */
    virtual void set_speed(SpeedType speed) = 0;
    /** Returns the last set speed of the locomotive. */
    virtual SpeedType get_speed() = 0;
    /** Returns the commanded speed of the locomotive. */
    virtual SpeedType get_commanded_speed() {
        return nan_to_speed();
    }
    /** Returns the actual speed of the locomotive, as provided by feedback
     * from the decoder. */
    virtual SpeedType get_actual_speed() {
        return nan_to_speed();
    }

    /** Sets the train to emergency stop. */
    virtual void set_emergencystop() = 0;

    /** Sets the value of a function.
     * @param address is a 24-bit address of the function to set. For legacy DCC
     * locomotives, see @ref TractionDefs for the address definitions (0=light,
     * 1-28= traditional function buttons).
     * @param value is the function value. For binary functions, any non-zero
     * value sets the function to on, zero sets it to off.*/
    virtual void set_fn(uint32_t address, uint16_t value) = 0;

    /** @returns the value of a function. */
    virtual uint16_t get_fn(uint32_t address) = 0;

    /** @returns the legacy (DCC) address of this train. This value is used in
     * determining the train's NMRAnet NodeID.
     * @TODO(balazs.racz) This function should not be here. Specifying the
     * NodeID should be more generic, but it is not clear what would be the
     * best interface for that.
     */
    virtual uint32_t legacy_address() = 0;
};

}  // namespace NMRAnet

#endif // _NMRANET_TRAININTERFACE_HXX_
 
