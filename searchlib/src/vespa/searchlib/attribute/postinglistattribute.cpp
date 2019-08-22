// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "postinglistattribute.h"
#include "loadednumericvalue.h"
#include "enumcomparator.h"
#include <vespa/vespalib/util/array.hpp>

namespace search {

using attribute::LoadedNumericValue;

template <typename P>
PostingListAttributeBase<P>::
PostingListAttributeBase(AttributeVector &attr,
                         EnumStoreBase &enumStore)
    : attribute::IPostingListAttributeBase(),
      _postingList(enumStore.getPostingDictionary(), attr.getStatus(),
                   attr.getConfig()),
      _attr(attr),
      _dict(enumStore.getPostingDictionary()),
      _esb(enumStore)
{ }

template <typename P>
PostingListAttributeBase<P>::~PostingListAttributeBase() = default;

template <typename P>
void
PostingListAttributeBase<P>::clearAllPostings()
{
    _postingList.clearBuilder();
    _attr.incGeneration(); // Force freeze
    auto itr = _dict.begin();
    EntryRef prev;
    while (itr.valid()) {
        EntryRef ref = itr.getData();
        if (ref.ref() != prev.ref()) {
            if (ref.valid()) {
                _postingList.clear(ref);
            }
            prev = ref;
        }
        itr.writeData(EntryRef());
        ++itr;
    }
    _attr.incGeneration(); // Force freeze
}


template <typename P>
void
PostingListAttributeBase<P>::fillPostingsFixupEnumBase(const LoadedEnumAttributeVector &loaded)
{
    clearAllPostings();
    uint32_t docIdLimit = _attr.getNumDocs();
    EnumStoreBase &enumStore = _esb;
    EntryRef newIndex;
    PostingChange<P> postings;
    if (loaded.empty()) {
        return;
    }
    uint32_t preve = 0;
    uint32_t refCount = 0;

    auto itr = _dict.begin();
    auto posting_itr = itr;
    assert(itr.valid());
    for (const auto& elem : loaded) {
        if (preve != elem.getEnum()) {
            assert(preve < elem.getEnum());
            enumStore.fixupRefCount(itr.getKey(), refCount);
            refCount = 0;
            while (preve != elem.getEnum()) {
                ++itr;
                assert(itr.valid());
                ++preve;
            }
            assert(itr.valid());
            if (enumStore.foldedChange(posting_itr.getKey(), itr.getKey())) {
                postings.removeDups();
                newIndex = EntryRef();
                _postingList.apply(newIndex,
                                   &postings._additions[0],
                                   &postings._additions[0] +
                                   postings._additions.size(),
                                   &postings._removals[0],
                                   &postings._removals[0] +
                                   postings._removals.size());
                posting_itr.writeData(newIndex);
                while (posting_itr != itr) {
                    ++posting_itr;
                }
                postings.clear();
            }
        }
        ++refCount;
        assert(elem.getDocId() < docIdLimit);
        (void) docIdLimit;
        postings.add(elem.getDocId(), elem.getWeight());
    }
    assert(refCount != 0);
    enumStore.fixupRefCount(itr.getKey(), refCount);
    postings.removeDups();
    newIndex = EntryRef();
    _postingList.apply(newIndex,
                       &postings._additions[0],
                       &postings._additions[0] + postings._additions.size(),
                       &postings._removals[0],
                       &postings._removals[0] + postings._removals.size());
    posting_itr.writeData(newIndex);
    enumStore.freeUnusedEnums(false);
}

template <typename P>
void
PostingListAttributeBase<P>::updatePostings(PostingMap &changePost,
                                            EnumStoreComparator &cmp)
{
    for (auto& elem : changePost) {
        auto& change = elem.second;
        EnumIndex idx = elem.first.getEnumIdx();
        auto dictItr = _dict.lowerBound(idx, cmp);
        assert(dictItr.valid() && dictItr.getKey() == idx);
        EntryRef newPosting = dictItr.getData();
        
        change.removeDups();
        _postingList.apply(newPosting,
                           &change._additions[0],
                           &change._additions[0] + change._additions.size(),
                           &change._removals[0],
                           &change._removals[0] + change._removals.size());
        
        _dict.thaw(dictItr);
        dictItr.writeData(newPosting);
    }
}

template <typename P>
bool
PostingListAttributeBase<P>::forwardedOnAddDoc(DocId doc,
                                               size_t wantSize,
                                               size_t wantCapacity)
{
    if (!_postingList._enableBitVectors) {
        return false;
    }
    if (doc >= wantSize) {
        wantSize = doc + 1;
    }
    if (doc >= wantCapacity) {
        wantCapacity = doc + 1;
    }
    return _postingList.resizeBitVectors(wantSize, wantCapacity);
}

template <typename P>
void
PostingListAttributeBase<P>::
clearPostings(attribute::IAttributeVector::EnumHandle eidx,
              uint32_t fromLid,
              uint32_t toLid,
              EnumStoreComparator &cmp)
{
    PostingChange<P> postings;

    for (uint32_t lid = fromLid; lid < toLid; ++lid) {
        postings.remove(lid);
    }

    EntryRef er(eidx);
    auto itr = _dict.lowerBound(er, cmp);
    assert(itr.valid());
    
    EntryRef newPosting = itr.getData();
    assert(newPosting.valid());
    
    _postingList.apply(newPosting,
                       &postings._additions[0],
                       &postings._additions[0] +
                       postings._additions.size(),
                       &postings._removals[0],
                       &postings._removals[0] +
                       postings._removals.size());
    _dict.thaw(itr);
    itr.writeData(newPosting);
}

template <typename P>
void
PostingListAttributeBase<P>::forwardedShrinkLidSpace(uint32_t newSize)
{
    (void) _postingList.resizeBitVectors(newSize, newSize);
}

template <typename P>
vespalib::MemoryUsage
PostingListAttributeBase<P>::getMemoryUsage() const
{
    return _postingList.getMemoryUsage();
}

template <typename P, typename LoadedVector, typename LoadedValueType,
          typename EnumStoreType>
PostingListAttributeSubBase<P, LoadedVector, LoadedValueType, EnumStoreType>::
PostingListAttributeSubBase(AttributeVector &attr,
                            EnumStore &enumStore)
    : Parent(attr, enumStore),
      _es(enumStore)
{
}

template <typename P, typename LoadedVector, typename LoadedValueType,
          typename EnumStoreType>
PostingListAttributeSubBase<P, LoadedVector, LoadedValueType, EnumStoreType>::
~PostingListAttributeSubBase() = default;

template <typename P, typename LoadedVector, typename LoadedValueType,
          typename EnumStoreType>
void
PostingListAttributeSubBase<P, LoadedVector, LoadedValueType, EnumStoreType>::
handleFillPostings(LoadedVector &loaded)
{
    clearAllPostings();
    EntryRef newIndex;
    PostingChange<P> postings;
    uint32_t docIdLimit = _attr.getNumDocs();
    _postingList.resizeBitVectors(docIdLimit, docIdLimit);
    if ( ! loaded.empty() ) {
        vespalib::Array<typename LoadedVector::Type> similarValues;
        auto value = loaded.read();
        LoadedValueType prev = value.getValue();
        for (size_t i(0), m(loaded.size()); i < m; i++, loaded.next()) {
            value = loaded.read();
            if (FoldedComparatorType::compareFolded(prev, value.getValue()) == 0) {
                // for single value attributes loaded[numDocs] is used
                // for default value but we don't want to add an
                // invalid docId to the posting list.
                if (value._docId < docIdLimit) {
                    postings.add(value._docId, value.getWeight());
                    similarValues.push_back(value);
                }
            } else {
                postings.removeDups();

                newIndex = EntryRef();
                _postingList.apply(newIndex,
                                   &postings._additions[0],
                                   &postings._additions[0] +
                                   postings._additions.size(),
                                   &postings._removals[0],
                                   &postings._removals[0] +
                                   postings._removals.size());
                postings.clear();
                if (value._docId < docIdLimit) {
                    postings.add(value._docId, value.getWeight());
                }
                similarValues[0]._pidx = newIndex;
                for (size_t j(0), k(similarValues.size()); j < k; j++) {
                    loaded.write(similarValues[j]);
                }
                similarValues.clear();
                similarValues.push_back(value);
                prev = value.getValue();
            }
        }

        postings.removeDups();
        newIndex = EntryRef();
        _postingList.apply(newIndex,
                           &postings._additions[0],
                           &postings._additions[0] +
                           postings._additions.size(),
                           &postings._removals[0],
                           &postings._removals[0] + postings._removals.size());
        similarValues[0]._pidx = newIndex;
        for (size_t i(0), m(similarValues.size()); i < m; i++) {
            loaded.write(similarValues[i]);
        }
    }
}


template <typename P, typename LoadedVector, typename LoadedValueType,
          typename EnumStoreType>
void
PostingListAttributeSubBase<P, LoadedVector, LoadedValueType, EnumStoreType>::
updatePostings(PostingMap &changePost)
{
    FoldedComparatorType cmpa(_es);

    updatePostings(changePost, cmpa);
}


template <typename P, typename LoadedVector, typename LoadedValueType,
          typename EnumStoreType>
void
PostingListAttributeSubBase<P, LoadedVector, LoadedValueType, EnumStoreType>::
clearPostings(attribute::IAttributeVector::EnumHandle eidx,
                       uint32_t fromLid,
                       uint32_t toLid)
{
    FoldedComparatorType cmp(_es);
    clearPostings(eidx, fromLid, toLid, cmp);
}



template class PostingListAttributeBase<AttributePosting>;
template class PostingListAttributeBase<AttributeWeightPosting>;

typedef SequentialReadModifyWriteInterface<LoadedNumericValue<int8_t> >
LoadedInt8Vector;

typedef SequentialReadModifyWriteInterface<LoadedNumericValue<int16_t> >
LoadedInt16Vector;

typedef SequentialReadModifyWriteInterface<LoadedNumericValue<int32_t> >
LoadedInt32Vector;

typedef SequentialReadModifyWriteInterface<LoadedNumericValue<int64_t> >
LoadedInt64Vector;

typedef SequentialReadModifyWriteInterface<LoadedNumericValue<float> >
LoadedFloatVector;

typedef SequentialReadModifyWriteInterface<LoadedNumericValue<double> >
LoadedDoubleVector;
                                                       

template class
PostingListAttributeSubBase<AttributePosting,
                            LoadedInt8Vector,
                            int8_t,
                            EnumStoreT<NumericEntryType<int8_t> > >;

template class
PostingListAttributeSubBase<AttributePosting,
                            LoadedInt16Vector,
                            int16_t,
                            EnumStoreT<NumericEntryType<int16_t> > >;

template class
PostingListAttributeSubBase<AttributePosting,
                            LoadedInt32Vector,
                            int32_t,
                            EnumStoreT<NumericEntryType<int32_t> > >;

template class
PostingListAttributeSubBase<AttributePosting,
                            LoadedInt64Vector,
                            int64_t,
                            EnumStoreT<NumericEntryType<int64_t> > >;

template class
PostingListAttributeSubBase<AttributePosting,
                            LoadedFloatVector,
                            float,
                            EnumStoreT<NumericEntryType<float> > >;

template class
PostingListAttributeSubBase<AttributePosting,
                            LoadedDoubleVector,
                            double,
                            EnumStoreT<NumericEntryType<double> > >;

template class
PostingListAttributeSubBase<AttributePosting,
                            attribute::LoadedStringVector,
                            const char *,
                            EnumStoreT<StringEntryType > >;

template class
PostingListAttributeSubBase<AttributeWeightPosting,
                            LoadedInt8Vector,
                            int8_t,
                            EnumStoreT<NumericEntryType<int8_t> > >;

template class
PostingListAttributeSubBase<AttributeWeightPosting,
                            LoadedInt16Vector,
                            int16_t,
                            EnumStoreT<NumericEntryType<int16_t> > >;

template class
PostingListAttributeSubBase<AttributeWeightPosting,
                            LoadedInt32Vector,
                            int32_t,
                            EnumStoreT<NumericEntryType<int32_t> > >;

template class
PostingListAttributeSubBase<AttributeWeightPosting,
                            LoadedInt64Vector,
                            int64_t,
                            EnumStoreT<NumericEntryType<int64_t> > >;

template class
PostingListAttributeSubBase<AttributeWeightPosting,
                            LoadedFloatVector,
                            float,
                            EnumStoreT<NumericEntryType<float> > >;

template class
PostingListAttributeSubBase<AttributeWeightPosting,
                            LoadedDoubleVector,
                            double,
                            EnumStoreT<NumericEntryType<double> > >;

template class
PostingListAttributeSubBase<AttributeWeightPosting,
                            attribute::LoadedStringVector,
                            const char *,
                            EnumStoreT<StringEntryType > >;


}
